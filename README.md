# RPI5 FUN

Terminal dashboard and fan control utility for Raspberry Pi 5.

![Auto Check](assets/screenshots/auto-check.png)
![Fans Control](assets/screenshots/fans-control.png)

## Quick Start

Copy, paste, run:

```bash
git clone https://github.com/DeziXsteroid/Raspberry-Fans-Control.git && cd Raspberry-Fans-Control && chmod +x run.sh && ./run.sh
```

This command:

- clones the repository
- configures the project with CMake
- builds the binary
- starts `Raspberry_Fun_Control`

## What It Does

- live monitoring for CPU temperature, CPU frequency, CPU load, per-core load, GPU load, RAM, load average and fan state
- manual fan control with direct PWM value input
- auto fan mode switch
- live terminal UI that works well over SSH
- configurable Linux paths from the Settings menu
- experimental GPIO memory mode for advanced fan-related control

## Build Manually

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
sudo ./build/Raspberry_Fun_Control
```

## Controls

### Main Menu

- `1` Start Auto Check
- `2` Fans Control
- `3` Settings
- `4` Help And Info
- `5` Exit

### Auto Check

- `Q` or `Й` stop live monitoring

### Fans Control

- `1` full speed
- `2` fan off
- `3` set custom PWM value
- `4` auto mode
- `5` memory mode ON
- `6` memory mode OFF
- `0` back

## Requirements

- Raspberry Pi 5
- Debian or Raspberry Pi OS 64-bit
- `cmake` 3.10+
- C++14 compiler
- `sudo` access for fan control operations

## macOS Note

You can manage, edit and publish this repository from macOS without any extra binary files.

- no DLL files are needed
- no precompiled Linux binaries need to be committed
- GitHub should contain only source code, scripts, screenshots and docs
- the app is built directly on Raspberry Pi or another Linux machine

## Project Files

- `main.cpp` terminal UI and navigation
- `Status_Sys.cpp` system metrics collection
- `FanControl_Sys.cpp` fan control logic
- `Temperature_Sys.cpp` CPU temperature reader
- `Paths_Sys.h` default runtime paths and startup settings
- `run.sh` one-command configure, build and run helper

## License

This repository includes the MIT License.

