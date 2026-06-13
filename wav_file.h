#ifndef WAV_FILE_H
#define WAV_FILE_H

#include <fstream>
#include <string>
#include <cstdint>
#include "audio_config.h"

// Owns the o/p wav file. Reserves 44-byte header on construction and writes the real header
// in the destructor. Header is always written even in case of early return. Non - copyable
class WavFile
{
public:
    WavFile(const std::string &path, const AudioConfig &config);
    ~WavFile();

    // Making it impossible to copy or duplicate
    WavFile(const WavFile &) = delete;
    WavFile &operator=(const WavFile &) = delete;

    // Append raw PCM bytes and track total for the header
    void write(const int16_t *data, size_t bytes);

private:
    void write_header();    // called by destructor

    std::ofstream file_;
    unsigned int sample_rate_;
    unsigned int channels_;
    unsigned int bits_per_sample_;
    size_t total_bytes_ = 0;         // running count of PCM bytes written
};

#endif