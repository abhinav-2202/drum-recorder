#include "wav_writer.h"
#include <cstdint>

void write_wav_header(std::ofstream &file,
                      unsigned int sample_rate,
                      unsigned int channels,
                      unsigned int bits_per_sample,
                      unsigned int data_size)
{
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);

    uint32_t chunk_size = 36 + data_size;
    uint32_t subchunk2_size = data_size;

    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM

    // Move file pointer to start
    file.seekp(0, std::ios::beg);

    // RIFF chunk
    file.write("RIFF", 4);
    file.write(reinterpret_cast<char*>(&chunk_size), 4);
    file.write("WAVE", 4);

    // fmt subchunk
    file.write("fmt ", 4);
    file.write(reinterpret_cast<char*>(&subchunk1_size), 4);
    file.write(reinterpret_cast<char*>(&audio_format), 2);
    file.write(reinterpret_cast<char*>(&channels), 2);
    file.write(reinterpret_cast<char*>(&sample_rate), 4);
    file.write(reinterpret_cast<char*>(&byte_rate), 4);
    file.write(reinterpret_cast<char*>(&block_align), 2);
    file.write(reinterpret_cast<char*>(&bits_per_sample), 2);

    // data subchunk
    file.write("data", 4);
    file.write(reinterpret_cast<char*>(&subchunk2_size), 4);
}