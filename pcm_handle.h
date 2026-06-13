#ifndef PCM_HANDLE_H
#define PCM_HANDLE_H

#include <alsa/asoundlib.h>
#include <stdexcept>
#include <string>
#include "audio_config.h"

// Throws error on negative ALSA code so a setup failure stops rather than giving bad audio.
// Throws for setup only and not for runtime. Inline is used to avoid multiple calls.
inline void alsa_check(int err, const char *what)
{
	if (err < 0)
		throw std::runtime_error(std::string(what) + ": " + snd_strerror(err));
}

// Owner of snd_pcm handle. Opens in constructor, closes in destructor. Ownership is movable
// but not copyable (only one owner at a time).
class PcmHandle
{
public:
	PcmHandle(const char *device, snd_pcm_stream_t stream)
		: stream_(stream)
	{
		alsa_check(snd_pcm_open(&handle_, device, stream, 0), "snd_pcm_open");
	}
	
	~PcmHandle()
	{
		if (handle_)                // null if this object is moved from
			snd_pcm_close(handle_);
	}
	
	PcmHandle(const PcmHandle &) = delete;              // forbidding copy
	PcmHandle &operator=(const PcmHandle &) = delete;
	
	// Move constructor : transfer ownership of device 'other' into this (newly built) object.
    // 'other' is set to null so exactly one device holds ownership
    PcmHandle(PcmHandle &&other) noexcept
		: handle_(other.handle_), stream_(other.stream_)
	{
		other.handle_ = nullptr;
	}
	
	// Move assignment : transfer ownership to an already existing object and may already own a
    // device. So we must close our current handle before taking other's.
    PcmHandle &operator=(PcmHandle &&other) noexcept
	{
		if (this != &other)                 // skipping if assigning to self
		{
			if(handle_)
				snd_pcm_close(handle_);
			
			handle_ = other.handle_;
			stream_ = other.stream_;
			other.handle_ = nullptr;
		}
		return *this;
	}
	
	void configure(AudioConfig &config)
	{
		snd_pcm_hw_params_t *params;
		snd_pcm_hw_params_alloca(&params);
		
		alsa_check(snd_pcm_hw_params_any(handle_, params), "hw_params_any");
		alsa_check(snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED), "set_access");
		alsa_check(snd_pcm_hw_params_set_format(handle_, params, config.format), "set_format");
		alsa_check(snd_pcm_hw_params_set_channels(handle_, params, config.channels), "set_channels");
		
		int dir = 0;
		alsa_check(snd_pcm_hw_params_set_rate_near(handle_, params, &config.sample_rate, &dir), "set_rate_near");
		alsa_check(snd_pcm_hw_params_set_period_size_near(handle_, params, &config.period_size, &dir), "set_period_size_near");
		
		snd_pcm_uframes_t buffer_size = config.period_size * config.periods;
		alsa_check(snd_pcm_hw_params_set_buffer_size_near(handle_, params, &buffer_size), "set_buffer_size_near");
		
		alsa_check(snd_pcm_hw_params(handle_, params), "hw_params commit");

		// Software params: required for linking, so that linked start() starts playback with silence
		// but when capture_.start() is called playback is started again leading to error. So we are
		// disabling autostart for playback
		snd_pcm_sw_params_t *sw;
		snd_pcm_sw_params_alloca(&sw);
		alsa_check(snd_pcm_sw_params_current(handle_, sw), "sw_params_current");
		alsa_check(snd_pcm_sw_params_set_start_threshold(handle_, sw, buffer_size + 1), "set_start_threshold");
		alsa_check(snd_pcm_sw_params(handle_, sw), "sw_params commit");
		
		snd_pcm_hw_params_get_rate(params, &config.sample_rate, &dir);
		snd_pcm_hw_params_get_period_size(params, &config.period_size, &dir);
		snd_pcm_hw_params_get_channels(params, &config.channels);
		//snd_pcm_uframes_t actual_buffer = 0;
		//snd_pcm_hw_params_get_buffer_size(params, &actual_buffer);
		//config.periods = actual_buffer / config.period_size;
	}
	
	// Keeps capture and playback on the same codec from drifting apart
	void link(PcmHandle &other) { alsa_check(snd_pcm_link(handle_, other.handle_), "snd_pcm_link"); }
	
	void prepare() { alsa_check(snd_pcm_prepare(handle_), "snd_pcm_prepare"); } // Readies the device
	void start() { alsa_check(snd_pcm_start(handle_), "snd_pcm_start"); }       // Begins the stream

    void recover()
    {
        snd_pcm_prepare(handle_);
        if (stream_ == SND_PCM_STREAM_CAPTURE)
            snd_pcm_start(handle_);
    }

        snd_pcm_t *get() const { return handle_; }

private:
        snd_pcm_t *handle_ = nullptr;
        snd_pcm_stream_t stream_;
};

#endif