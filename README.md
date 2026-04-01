# Drum Recorder

A low-latency, embedded drum recording device inspired by the Yamaha EAD10, built for the Indian market at a low cost.

## Overview

This project implements a real-time stereo audio pipeline on Raspberry Pi using ALSA, capturing a drum mic placed on the kick drum and snare piezo sensor simultaneously. The recorded audio is saved as timestamped WAV files.

## Architecture
```
Mic (on kick drum) ────────────┐
                               ├──► Stereo Capture ──► Circular Buffer ──► WAV File
Piezo (snare trigger) ─────────┘                            │
                                                            └──► Real-time Playback
```

3 concurrent threads:
- **Capture thread** — reads raw PCM frames from ALSA hardware device
- **Playback thread** — writes frames to output for real-time monitoring
- **Write thread** — independently writes frames to timestamped WAV file

## Hardware

|    Component    |               Details                 |
|-----------------|---------------------------------------|
| Board           | Raspberry Pi 4B 4GB                   |
| Audio Interface | USB Sound Card (S16_LE, 48000Hz)      |
| Target (final)  | Raspberry Pi Zero 2W + WM8960 I2S HAT |
| Input 1         | Mic on kick drum                      |
| Input 2         | Piezo sensor on snare drum            | 

## Build
```bash
sudo apt install libasound2-dev g++ make
make
```

## Run
```bash
./drumrecorder
# Press ENTER to stop recording
# Output saved as YYYY-MM-DD_HH-MM-SS.wav
```

## Status

Work in progress. Current state:
- [x] Stereo ALSA capture and playback
- [x] 3-thread pipeline with circular buffer
- [x] WAV file output with timestamped filenames
- [x] xrun recovery
- [x] USB sound card integration and testing
- [ ] Piezo envelope detection and mic blending DSP
- [ ] WM8960 I2S HAT integration
- [ ] GPIO button for start/stop
- [ ] Systemd auto-start on boot
