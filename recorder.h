#ifndef RECORDER_H
#define RECORDER_H

#include <memory>
#include <atomic>
#include <string>

#include "audio_config.h"
#include "pcm_handle.h"
#include "thread_safe_buffer.h"
#include "wav_file.h"

// Owns the entire recording pipeline: two ALSA handles, two buffers, output file, and four worker threads.
// Construction opens and configures hardware and allocates buffers. run() arms the devices, launches the threads,
// and blocks until user stops recording. Everything is automatic: when Recorder is destroyed, buffers are freed,
// WavFile writes header, and PcmHandle closes deivces.
class Recorder
{
public:
    Recorder(const std::string &device, const AudioConfig &config);

    // Non-copyable: owns unique hardware handles and an open file
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    void run();

private:
    // Threads (one per worker)
    void capture_loop();        // producer: ALSA -> both buffers
    void playback_loop();       // consumer: capture_buffer -> ALSA playback
    void write_loop();          // consumer: write_buffer -> WAV file
    void input_loop();          // waits for ENTER, then clears recording_

    AudioConfig config_;
    PcmHandle capture_;
    PcmHandle playback_;

    // Buffers and file are built in constructor, after configure() has written real parameters into config_. -----------?
    // Held by unique_ptr only because they cannot be constructed in initializer list as their sizes are unknown
    // till configure runs. Single ownership.
    std::unique_ptr<ThreadSafeBuffer> capture_buffer_;
    std::unique_ptr<ThreadSafeBuffer> write_buffer_;
    std::unique_ptr<WavFile> wav_;

    int bytes_per_sample_ = 0;

    std::atomic<bool> recording_{true};
    std::atomic<size_t> xruns_capture_{0};
    std::atomic<size_t> xruns_playback_{0};
};

#endif