#include "recorder.h"

#include <alsa/asoundlib.h>
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>
#include <ctime>

using namespace std::chrono_literals;   // enables the 5ms literal

namespace
{
    // Building a timestamped WAV filename
    std::string make_filename()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S.wav", std::localtime(&t));
        return std::string(buf);
    }
}

Recorder::Recorder(const std::string &device, const AudioConfig &config)
    : config_(config),
    capture_(device.c_str(), SND_PCM_STREAM_CAPTURE),       // open both devices here
    playback_(device.c_str(), SND_PCM_STREAM_PLAYBACK)      // opening needs no values
{
    // Configure capture first. Writes actual hardware parameters back into config_
    capture_.configure(config_);
    playback_.configure(config_);

    bytes_per_sample_ = snd_pcm_format_physical_width(config_.format) / 8;

    std::cout << "Negotiated: " << config_.sample_rate << " Hz, "
                                << config_.channels << " ch, period "
                                << config_.period_size << ", periods "
                                << config_.periods << "(buffer "
                                << config_.period_size * config_.periods
                                << " frames)\n";

    // Now config_ holds real parameters. Size the buffer and open the file
    size_t chunk = config_.period_size * config_.channels;
    capture_buffer_ = std::make_unique<ThreadSafeBuffer>(8, chunk, config_.channels);
    write_buffer_   = std::make_unique<ThreadSafeBuffer>(8, chunk, config_.channels);
    wav_            = std::make_unique<WavFile>(make_filename(), config_);
}

void Recorder::run()
{
    capture_.prepare();
    playback_.prepare();
    capture_.link(playback_);

    // Due to linking both streams start at once. Playback starting empty instantly leads to underrun
    // So we fill it with silence first. Fill up with one period of headroom.
    std::vector<int16_t> silence(config_.period_size * config_.channels, 0);
    for (unsigned int i = 0; i < config_.periods - 1; i++)
        snd_pcm_writei(playback_.get(), silence.data(), config_.period_size);

    capture_.start();
    //playback_.start();    // playback auto starts on first write. starting here leads to underruns

    // Threads are local. Launching and joining them here keeps their lifetime bounded
    // by this function
    std::thread input(&Recorder::input_loop, this);
    std::thread capture(&Recorder::capture_loop, this);
    std::thread playback(&Recorder::playback_loop, this);
    std::thread writer(&Recorder::write_loop, this);

    input.join();       // returns when user presses ENTER (clears recording_)
    capture.join();     // capture sees recording_ == false and stops
    playback.join();    // drains capture_buffer_, then exits
    writer.join();      // drains write_buffer_, then exits

    // Letting audio queued in buffer finish before closing device
    snd_pcm_drain(playback_.get());

    std::cout << "Done. xruns(capture/playback): "
              << xruns_capture_ << "/" << xruns_playback_
              << ", drops(playback/write): "
              << capture_buffer_->drops() << "/" << write_buffer_->drops() << "\n";
}

void Recorder::capture_loop()
{
    std::vector<int16_t> local(config_.period_size * config_.channels);     // allocated once

    while (recording_)
    {
        int frames = snd_pcm_readi(capture_.get(), local.data(), config_.period_size);

        if (frames == -EPIPE)       // overrun: card filled faster than we drained
        {
            capture_.recover();
            xruns_capture_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        else if (frames < 0)        // other read error: skip this period
        {
            continue;
        }

        // Same audio goes to both consumers. push() copies, counts drops, and
        // notifies internally that there is no lock
        capture_buffer_->push(local, frames);
        write_buffer_->push(local, frames);
    }
}

void Recorder::playback_loop()
{
    AudioChunk chunk;

    while(true)
    {
        if (capture_buffer_->pop_wait(chunk, 5ms))
        {
            int written = snd_pcm_writei(playback_.get(), chunk.data.data(), chunk.frames);
            if (written == -EPIPE)              // underrun: ran out of audio to play
            {
                playback_.recover();
                xruns_playback_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        else if (!recording_)                   // timed out empty and recording stopped
        {
            break;
        }
    }
}

void Recorder::write_loop()
{
    AudioChunk chunk;

    while(true)
    {
        if (write_buffer_->pop_wait(chunk, 5ms))
        {
            size_t bytes = chunk.frames * config_.channels * bytes_per_sample_;
            wav_->write(chunk.data.data(), bytes);
        }
        else if (!recording_)
        {
            break;
        }
    }
}

void Recorder::input_loop()
{
    std::cout << "Press ENTER to stop recording...\n";
    std::cin.get();
    recording_ = false;
}