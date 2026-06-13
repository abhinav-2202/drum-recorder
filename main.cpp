#include "recorder.h"
#include <alsa/asoundlib.h>
#include <iostream>
#include <stdexcept>

int main()
{
    AudioConfig config
    {
        48000,                      // sample_rate (adjustable)
        2,                          // channels - stereo
        SND_PCM_FORMAT_S16_LE,      // 16 bit little endian
        512,                        // period_size
        8                           // periods - 8 (4 was leading to a lot of playback xruns)
    };

    try
    {
        Recorder recorder("hw:1,0", config);        // set to WM8960
        recorder.run();
    }
    catch(const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}