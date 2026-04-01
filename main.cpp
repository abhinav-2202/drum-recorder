#include <alsa/asoundlib.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>

#include "wav_writer.h"

struct AudioConfig                      //Structure to hold all the audio parameters
{
    unsigned int sample_rate;
    unsigned int capture_channels;      //taking seperate channel as soundcard is asymmetric
    unsigned int playback_channels;
    snd_pcm_format_t format;
    snd_pcm_uframes_t period_size;
    unsigned int periods;
};

std::mutex mtx;                                 //protects queue from simulataneous access
std::condition_variable cv;                     //notifies playback thread when data is ready

std::mutex write_mtx;                            //writes data into output file
std::condition_variable write_cv;

struct AudioChunk
{
    std::vector<int16_t> data;
    int frames;
};

struct CircularBuffer                           //Using circular buffer as audio queue as it has fixed memory
{
    std::vector<AudioChunk> buffer;
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    size_t capacity;
    size_t channels;

    CircularBuffer(size_t cap, size_t chunk_size, size_t ch) : capacity(cap), channels(ch)
    {
        buffer.resize(cap);
        for (auto &slot : buffer)
        {
            slot.data.resize(chunk_size);       //chunk_size = period_size * channels
            slot.frames = 0;
        }
    }

    bool push(const std::vector<int16_t> &input, int frames)
    {
        if (count == capacity)
        {
            std::cerr << "Buffer FULL - dropping\n";
            return false; // buffer full
        }

        //copy only valid data
        int samples = frames * channels;
        for (int i = 0; i < samples; i++)
        {
            buffer[head].data[i] = input[i];
        }
        
        buffer[head].frames = frames;
        head = (head + 1) % capacity;
        count++;

        return true;
    }

    bool pop(AudioChunk &out)
    {
        if (count == 0)
        {
            return false; // buffer empty
        }

        out.frames = buffer[tail].frames;   //copying metadata
        
        int samples = out.frames * channels;
        out.data.assign(buffer[tail].data.begin(), buffer[tail].data.begin() + samples);

        tail = (tail + 1) % capacity;
        count--;

        return true;
    }

    bool empty() const { return count == 0; }
    bool full() const { return count == capacity; }
};

int main()
{
    AudioConfig config {                //Defining Audio configuration
        48000,                          //sample_rate
        1,                              //capture = mono
        2,                              //playback = stereo
        SND_PCM_FORMAT_S16_LE,          //format = 24 bit
        512,                           //period_size
        4,                              //periods
    };

    int bytes_per_sample = snd_pcm_format_physical_width(config.format) / 8;

    CircularBuffer audio_buffer(8, config.period_size * config.capture_channels, config.capture_channels);  //buffer for playback
    CircularBuffer write_buffer(8, config.period_size * config.capture_channels, config.capture_channels);  //buffer for writing to output file 

    // ----------------------- Open ALSA device -----------------------

    snd_pcm_t *capture_handle;      //acts as connection to audio devices
    snd_pcm_t *playback_handle;

    snd_pcm_hw_params_t *params_capture;    //configuration structure to describe audio format 
    snd_pcm_hw_params_t *params_playback;

    int dir;                        //required by API to negotiate sample_rate

    //open capture device (for now we use default device)
    int err;

    err = snd_pcm_open(&capture_handle, "hw:1,0", SND_PCM_STREAM_CAPTURE, 0);   //Using card 1 device 0 - HDA Analog

    if (err < 0)
    {
        std::cerr << "Cannot open capture device\n";
        return 1;
    }

    err = snd_pcm_open(&playback_handle, "hw:1,0", SND_PCM_STREAM_PLAYBACK, 0); //Using card 1 device 0 - HDA Analog

    if (err < 0)
    {
        std::cerr << "Cannot open playback device\n";
        return 1;
    }

    snd_pcm_hw_params_alloca(&params_capture);          //creating a parameter object
    snd_pcm_hw_params_alloca(&params_playback);

    snd_pcm_hw_params_any(capture_handle, params_capture);      //initialize parameters
    snd_pcm_hw_params_any(playback_handle, params_playback);

    // ----------------------- Configure capture hardware -----------------------
    snd_pcm_hw_params_set_access(capture_handle, params_capture, SND_PCM_ACCESS_RW_INTERLEAVED);    //Interleaved means stereo is stored LRLRLR
    snd_pcm_hw_params_set_format(capture_handle, params_capture, config.format);
    snd_pcm_hw_params_set_channels(capture_handle, params_capture, config.capture_channels);
    snd_pcm_hw_params_set_rate_near(capture_handle, params_capture, &config.sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(capture_handle, params_capture, &config.period_size, &dir);

    snd_pcm_uframes_t buffer_size = config.period_size * config.periods;    //Explicitly defining buffer size
    snd_pcm_hw_params_set_buffer_size_near(capture_handle, params_capture, &buffer_size);

    err = snd_pcm_hw_params(capture_handle, params_capture);

    if (err < 0)
    {
        std::cerr << "Cannot set capture parameters\n";
        return 1;
    }

    snd_pcm_hw_params_get_rate(params_capture, &config.sample_rate, &dir);
    snd_pcm_hw_params_get_period_size(params_capture, &config.period_size, &dir);
    snd_pcm_hw_params_get_channels(params_capture, &config.capture_channels);

    std::cout << "Actual Capture Params - Rate: " << config.sample_rate
              << " Period: " << config.period_size
              << " Channels: " << config.capture_channels << "\n";         //Printing actual capture parameters

    // ----------------------- Configure playback hardware -----------------------
    snd_pcm_hw_params_set_access(playback_handle, params_playback, SND_PCM_ACCESS_RW_INTERLEAVED);    //Interleaved means stereo is stored LRLRLR
    snd_pcm_hw_params_set_format(playback_handle, params_playback, config.format);
    snd_pcm_hw_params_set_channels(playback_handle, params_playback, config.playback_channels);
    snd_pcm_hw_params_set_rate_near(playback_handle, params_playback, &config.sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(playback_handle, params_playback, &config.period_size, &dir);

    snd_pcm_hw_params_set_buffer_size_near(playback_handle, params_playback, &buffer_size);

    err = snd_pcm_hw_params(playback_handle, params_playback);

    if (err < 0)
    {
        std::cerr << "Cannot set playback parameters\n";
        return 1;
    }

    /*err = snd_pcm_link(capture_handle, playback_handle);        //creating a link b/w capture and playback

    if (err < 0)
    {
        std::cerr << "Cannot link PCMs\n";
    }

    snd_pcm_start(capture_handle);*/

    // no link — asymmetric channels prevent linking
    snd_pcm_prepare(capture_handle);
    snd_pcm_prepare(playback_handle);
    snd_pcm_start(capture_handle);
    snd_pcm_start(playback_handle);

    //Printing actual playback parameters
    unsigned int pb_rate;
    snd_pcm_uframes_t pb_period;
    unsigned int pb_channels;

    snd_pcm_hw_params_get_rate(params_playback, &pb_rate, &dir);
    snd_pcm_hw_params_get_period_size(params_playback, &pb_period, &dir);
    snd_pcm_hw_params_get_channels(params_playback, &pb_channels);

    std::cout << "Actual Playback Params - Rate: " << pb_rate
              << " Period: " << pb_period
              << " Channels: " << pb_channels << "\n";
    
    if (pb_rate != config.sample_rate || pb_period != config.period_size)
        std::cerr << "WARNING: capture/playback params mismatch — expect drift\n";

    std::vector<int16_t> buffer(config.period_size * config.capture_channels);  //creating audio buffer

    // ----------------------- Open WAV file -----------------------
    auto now = std::chrono::system_clock::now();                //timestamped file name
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm *tm_info = std::localtime(&t);

    char filename[64];
    std::strftime(filename, sizeof(filename), "%Y-%m-%d_%H-%M-%S.wav", tm_info);

    std::ofstream file(filename, std::ios::binary);

    if (!file)
    {
        std::cerr << "Cannot open output file\n";
        return 1;
    }

    // Reserve header space
    char header[44] = {0};
    file.write(header, 44);

    size_t total_data_bytes = 0;

    // Thread to stop recording
    std::atomic<bool> recording(true);

    std::thread input_thread([&recording]() {
        std::cout << "Press ENTER to stop recording...\n";
        std::cin.get();
        recording = false;
    });

    // ----------------------- Recording loop -----------------------
    
    std::thread capture_thread([&]() {
    std::vector<int16_t> local_buffer(config.period_size * config.capture_channels);    //creating a local buffer for this loop
    while (recording)
    {
        int frames_read = snd_pcm_readi(capture_handle, local_buffer.data(), config.period_size); //reading from ALSA

        //DEBUG
        //std::cout << "frames_read: " << frames_read << "\n";

        if (frames_read == -EPIPE)              //handling buffer overrun
        {
            snd_pcm_prepare(capture_handle);
            snd_pcm_start(capture_handle);
            continue;
        }
        else if (frames_read < 0)
        {
            std::cerr << "Capture error: " << snd_strerror(frames_read) << "\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mtx);      //basically locks, does work and then unlocks
            if (!audio_buffer.push(local_buffer, frames_read))  //we move the local buffer into the audio queue
            {
                std::cerr << "Playback buffer full, dropping audio\n";
            }
        }

        cv.notify_one();        //notify playback that data is ready

        {
            std::lock_guard<std::mutex> lock(write_mtx);
            if (!write_buffer.push(local_buffer, frames_read))
            {
                std::cerr << "Write buffer full, dropping audio\n";
            }
        }

        write_cv.notify_one();
    }
    });

    std::thread playback_thread([&]() {
    while (recording || !audio_buffer.empty())
    {
        AudioChunk chunk;

        {
            std::unique_lock<std::mutex> lock(mtx);

            cv.wait_for(lock, std::chrono::milliseconds(5), [&] {return !audio_buffer.empty();});   //waits until data is available or recording is stopped

            if (!audio_buffer.pop(chunk))      //takes data from audio queue
                continue;
        }

        int frames = chunk.frames;

        if (frames <= 0 || frames > (int)config.period_size)
        {
            std::cerr << "Invalid frame count: " << frames << "\n";
            continue;
        }
        
        //int frames_written = snd_pcm_writei(playback_handle, chunk.data.data(), frames);    //writes to ALSA
        
        // upmix mono to stereo — duplicate each sample to both L and R channels
        std::vector<int16_t> stereo_buffer(frames * 2);
        for (int i = 0; i < frames; i++)
        {
            stereo_buffer[i * 2]     = chunk.data[i];   // left
            stereo_buffer[i * 2 + 1] = chunk.data[i];   // right
        }

        int frames_written = snd_pcm_writei(playback_handle, stereo_buffer.data(), frames);
        
        if (frames_written == -EPIPE)               //Underrun condition, speaker ran out of data
        {
            snd_pcm_prepare(playback_handle);
        }
        else if (frames_written < 0)
        {
            std::cerr << "Playback error" << snd_strerror(frames_written) << "\n";
        }
    }
    });

    std::thread write_thread([&]() {
    while (recording || !write_buffer.empty())
    {
        AudioChunk chunk;

        {
            std::unique_lock<std::mutex> lock(write_mtx);
            write_cv.wait_for(lock, std::chrono::milliseconds(5),[&] { return !write_buffer.empty(); });

            if (!write_buffer.pop(chunk))
                continue;
        }

        size_t bytes = chunk.frames * config.capture_channels * bytes_per_sample;
        file.write(reinterpret_cast<char*>(chunk.data.data()), bytes);
        total_data_bytes += bytes;
    }
    });

    input_thread.join();
    capture_thread.join();
    playback_thread.join();
    write_thread.join();

    // ----------------------- Write WAV header -----------------------
    file.seekp(0, std::ios::beg);
    write_wav_header(
    file,
    config.sample_rate,
    config.capture_channels,
    bytes_per_sample * 8,
    total_data_bytes);

    snd_pcm_drain(playback_handle);
    snd_pcm_close(playback_handle);

    snd_pcm_drain(capture_handle);
    snd_pcm_close(capture_handle);

    file.close();

    std::cout << "Recording finished.\n";

    return 0;
}


/*
TASKS

1. Change to stereo
2. 
3. 


*/
