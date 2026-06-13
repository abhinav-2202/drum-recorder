#include "wav_file.h"
#include <stdexcept>
#include <alsa/asoundlib.h>

WavFile::WavFile(const std::string &path, const AudioConfig &config)
    :file_(path, std::ios::binary), 
    sample_rate_(config.sample_rate),
    channels_(config.channels),
    bits_per_sample_(snd_pcm_format_physical_width(config.format))
{
    if(!file_)
        throw std::runtime_error("Cannot open output file: " + path);

    // Reserve space for the header
    char placeholder[44] = {0};
    file_.write(placeholder, sizeof(placeholder));
}

WavFile::~WavFile()
{
    write_header();
}

void WavFile::write(const int16_t *data, size_t bytes)
{
    file_.write(reinterpret_cast<const char *>(data), bytes);
    total_bytes_ += bytes;
}

void  WavFile::write_header()
{
    // WAV is little-endian (reads right to left). The 2 bytes fields are written from unint16_t locals
    // so that write matches field size exactly.
    uint32_t data_size   = static_cast<uint32_t>(total_bytes_);
    uint16_t channels16  = static_cast<uint16_t>(channels_);
    uint16_t bits16      = static_cast<uint16_t>(bits_per_sample_);
    uint32_t byte_rate   = sample_rate_ * channels_ * (bits_per_sample_ / 8);
    uint16_t block_align = static_cast<uint16_t>(channels_ * (bits_per_sample_ / 8));
    uint32_t chunk_size = 36 + data_size;
    uint32_t subchunk1 = 16;    // size of fmt block
    uint16_t audio_format = 1;  // 1 = PCM

    file_.seekp(0, std::ios::beg);  // back to start to overwrite placeholder

    // RIFF chunk
    file_.write("RIFF", 4);
    file_.write(reinterpret_cast<char *>(&chunk_size), 4);
    file_.write("WAVE", 4);

    // fmt subchunk
    file_.write("fmt ", 4);
    file_.write(reinterpret_cast<char *>(&subchunk1), 4);
    file_.write(reinterpret_cast<char *>(&audio_format), 2);
    file_.write(reinterpret_cast<char *>(&channels16), 2);
    file_.write(reinterpret_cast<char *>(&sample_rate_), 4);
    file_.write(reinterpret_cast<char *>(&byte_rate), 4);
    file_.write(reinterpret_cast<char *>(&block_align), 2);
    file_.write(reinterpret_cast<char *>(&bits16), 2);

    // data subchunk
    file_.write("data", 4);
    file_.write(reinterpret_cast<char *>(&data_size), 4);
}