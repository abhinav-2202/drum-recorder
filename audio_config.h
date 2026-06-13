#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include <alsa/asoundlib.h>

// Shared audio parameters. Some values maybe overwritten by configure()
// with the values the hardware actually negotiated.
struct AudioConfig
{
    unsigned int sample_rate;
    unsigned int channels;
    snd_pcm_format_t format;
    snd_pcm_uframes_t period_size;
    unsigned int periods;           // buffer_size = period_size * periods
};

#endif