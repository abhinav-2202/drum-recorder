# Drum Recorder

A low-latency, embedded drum recording device inspired by the Yamaha EAD10, built for the Indian market at a low cost.

## Overview

This project implements a real-time stereo audio pipeline on Raspberry Pi using ALSA, capturing a drum mic placed on the kick drum and a snare piezo sensor simultaneously. The recorded audio is saved as timestamped WAV files.

## Architecture

```
Mic (on kick drum) ────────────┐
                               ├──► Stereo Capture ──► Circular Buffer ──► WAV File
Piezo (snare trigger) ─────────┘                            │
                                                            └──► Real-time Playback
```

3 concurrent threads coordinated via condition variables and a shared circular buffer:

* **Capture thread** — reads raw PCM frames from the ALSA hardware device, driven by hardware timing
* **Playback thread** — pulls frames for real-time monitoring output
* **Write thread** — independently writes frames to a timestamped WAV file, decoupling slow disk I/O from the audio path

## Hardware

| Component | Details |
| --- | --- |
| Board | Raspberry Pi 4B 4GB |
| Audio Interface (current) | Generic USB Sound Card (S16_LE, 48000Hz) for testing |
| Audio Interface (target) | WM8960 I2S HAT (just integrated) |
| Final Board (target) | Raspberry Pi Zero 2W |
| Input 1 | Mic on kick drum |
| Input 2 | Piezo sensor on snare drum |

## Build

```
sudo apt install libasound2-dev g++ make
make
```

## Run

```
./drumrecorder
# Press ENTER to stop recording
# Output saved as YYYY-MM-DD_HH-MM-SS.wav
```

## Status

### Completed
- [x] Stereo ALSA capture and playback
- [x] 3-thread pipeline with circular buffer (condition-variable-coordinated)
- [x] WAV file output with timestamped filenames
- [x] xrun recovery
- [x] USB sound card integration and testing
- [x] WM8960 I2S HAT integration at OS level (device tree, arecord/aplay verified)

### In Progress / Next
- [ ] Pipeline integration with WM8960 (replacing USB sound card path)
- [ ] Piezo envelope detection and mic blending DSP
- [ ] Migration to Pi Zero 2W as final target hardware
- [ ] GPIO button for start/stop
- [ ] Systemd auto-start on boot

## Upstream Contribution

Debugging this project led directly to a merged patch in the mainline Linux kernel:

> [ALSA: usb-audio: Add quirk for PreSonus AudioBox USB (0x194f:0x0301)](https://lore.kernel.org/linux-sound/20260410143335.5974-1-abhi220204@gmail.com/)

While testing with a PreSonus AudioBox USB, I found the device only advertised the S24_3LE format and had no entry in the kernel's USB audio quirks table. The patch adds an entry to `sound/usb/quirks-table.h` and was reviewed and accepted by Takashi Iwai, the ALSA subsystem maintainer.

## Why I Built This

I'm a drummer. Commercial multi-track drum recording solutions (like the Yamaha EAD10) cost ₹40k+. I wanted something at a fraction of that price, built around hardware accessible to amateur drummers. This is also a deep-dive into embedded Linux audio for me.
