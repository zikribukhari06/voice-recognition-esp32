# 🎙️ ESP32-S3 Offline Voice Recognition

Offline wake word and voice command recognition on an **ESP32-S3** using an **INMP441** I2S MEMS microphone, built with **ESP-IDF** and **ESP-SR**. No cloud, no internet required.

![demo](docs/demo.gif)
<!-- Replace with a short GIF/video: say the wake word → LED/relay reacts -->

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-red)
![Target](https://img.shields.io/badge/target-ESP32--S3-blue)
![License](https://img.shields.io/badge/license-MIT-green)

---

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
- [Hardware](#hardware)
- [Wiring](#wiring)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Supported Commands](#supported-commands)
- [Results](#results)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Acknowledgements](#acknowledgements)
- [License](#license)

---

## Features

- Real-time audio capture from INMP441 over I2S (16 kHz, 32-bit frames → 16-bit PCM)
- Audio front-end: noise suppression and voice activity detection (ESP-SR AFE)
- Offline wake word detection (WakeNet)
- Offline command recognition (MultiNet)
- Dual-core pipeline using FreeRTOS tasks and queues (capture and inference run in parallel)
- Modular ESP-IDF components: `audio_capture`, `recognition`, `actuator`
- Indonesian commands via phonetic mapping *(see [Supported Commands](#supported-commands))*

## How It Works

```
┌──────────┐  I2S   ┌───────────────┐   ┌──────────┐   ┌───────────────┐   ┌──────────┐
│ INMP441  │ ─────▶ │ audio_capture │──▶│  AFE     │──▶│ WakeNet →     │──▶│ actuator │
│ (MEMS)   │ 16 kHz │ (Core 0)      │   │ NS + VAD │   │ MultiNet      │   │ LED/Relay│
└──────────┘        └───────────────┘   └──────────┘   │ (Core 1)      │   └──────────┘
                                                       └───────────────┘
```

1. **Sensing**: the INMP441's MEMS diaphragm vibrates with sound pressure. The chip digitizes it with a built-in sigma-delta ADC and streams it as I2S data.
2. **Capture**: the ESP32-S3 reads I2S via DMA and converts samples to 16-bit PCM.
3. **Front-end**: AFE cleans the audio and detects whether someone is speaking.
4. **Wake word**: WakeNet listens continuously with low CPU cost.
5. **Command**: after the wake word, MultiNet matches speech against a predefined command list.
6. **Action**: the recognized command ID triggers a GPIO action.

## Hardware

| Component | Notes |
|---|---|
| ESP32-S3 dev board | Must have **PSRAM** (e.g. N16R8) for ESP-SR |
| INMP441 I2S microphone module | 3.3 V only, do **not** use 5 V |
| LED / relay module | For command feedback |
| Jumper wires | Keep mic wires short (< 20 cm) |

## Wiring

| INMP441 | ESP32-S3 | Notes |
|---|---|---|
| VDD | 3V3 | |
| GND | GND | |
| SCK (BCLK) | GPIO 4 | |
| WS (LRCL) | GPIO 5 | |
| SD (DOUT) | GPIO 6 | |
| L/R | GND | Left channel. Tie to 3V3 for right channel |

| Output | ESP32-S3 |
|---|---|
| LED | GPIO 15 *(change as needed)* |

![wiring](docs/wiring.jpg)
<!-- Add a photo or Fritzing diagram of your actual setup -->

> **Note:** avoid GPIO 26–37 (flash/PSRAM), 0/3/45/46 (strapping), and 19/20 (USB) on S3 boards.

## Project Structure

```
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   └── main.c
├── components/
│   ├── audio_capture/     # I2S driver setup + PCM ring buffer
│   ├── recognition/       # AFE, WakeNet, MultiNet pipeline
│   └── actuator/          # GPIO control for LED/relay
├── docs/
│   ├── demo.gif
│   └── wiring.jpg
├── LICENSE
└── README.md
```

## Getting Started

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) installed and exported
- ESP32-S3 board with PSRAM
- USB cable

### Build and Flash

```bash
git clone https://github.com/<your-username>/<your-repo>.git
cd <your-repo>

idf.py set-target esp32s3
idf.py menuconfig        # see Configuration below
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Windows: -p COM3
```

Exit the monitor with `Ctrl + ]`.

## Configuration

Open `idf.py menuconfig` and check:

- **Component config → ESP PSRAM**: enable, mode matching your board (Octal for R8 variants)
- **Serial flasher config → Flash size**: match your board (e.g. 16 MB)
- **ESP Speech Recognition**: select wake word model and command model
- **Partition Table**: use the custom `partitions.csv` (models are stored in a dedicated partition)

I2S pins are defined in `components/audio_capture/include/audio_capture.h`:

```c
#define I2S_BCLK_GPIO  GPIO_NUM_4
#define I2S_WS_GPIO    GPIO_NUM_5
#define I2S_DIN_GPIO   GPIO_NUM_6
#define I2S_SAMPLE_RATE 16000
```

## Supported Commands

> Edit this table to match your command list.

| Spoken (phonetic) | Meaning | Action |
|---|---|---|
| `NYAH LAH KAN LAM PU` | "nyalakan lampu" | LED ON |
| `MAH TEE KAN LAM PU` | "matikan lampu" | LED OFF |

ESP-SR MultiNet does not officially support Indonesian, so commands are written as English-style phonetic spellings. Accuracy varies by speaker and should be validated (see [Results](#results)).

## Results

> Fill these in with your own measurements. Real numbers make a project stand out.

| Metric | Value |
|---|---|
| Wake word detection rate | _TBD_ % (n = _TBD_ trials) |
| False wake-ups | _TBD_ per hour |
| Command accuracy | _TBD_ % (n = _TBD_ trials, _TBD_ speakers) |
| End-to-end latency (speech end → action) | _TBD_ ms |
| Free internal RAM / PSRAM at runtime | _TBD_ KB / _TBD_ MB |
| CPU usage (core 0 / core 1) | _TBD_ % / _TBD_ % |

**Test conditions:** _distance from mic, room noise level, number of speakers._

## Troubleshooting

| Problem | Likely cause |
|---|---|
| Mic data is always 0 | L/R pin not matching the slot setting in code (`LEFT` vs `RIGHT`) |
| Very noisy or clipped audio | Long wires, or bit shift is wrong when converting 32-bit → 16-bit |
| Boot loop or PSRAM init failure | PSRAM mode in `menuconfig` doesn't match the board (Quad vs Octal) |
| Model not found at boot | Model partition missing or not flashed |
| Wake word rarely triggers | Mic too far, high background noise, or wrong gain |

## Roadmap

- [x] I2S capture from INMP441
- [x] Wake word + offline commands
- [ ] Custom Indonesian command model (Edge Impulse / own dataset)
- [ ] MQTT / WiFi dashboard for status and control
- [ ] Optional cloud STT fallback for free-form sentences
- [ ] Accuracy benchmark across multiple speakers

## Acknowledgements

- [Espressif ESP-SR](https://github.com/espressif/esp-sr) and [esp-skainet](https://github.com/espressif/esp-skainet)
- [ESP-IDF](https://github.com/espressif/esp-idf)
- InvenSense INMP441 datasheet

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for details.

---

<sub>Built by <a href="https://github.com/your-username">@your-username</a></sub>
