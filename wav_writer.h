#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <fstream>

void write_wav_header(std::ofstream &file,
                      unsigned int sample_rate,
                      unsigned int channels,
                      unsigned int bits_per_sample,
                      unsigned int data_size);

#endif