<h1 align="center">Raspberry Pi5 Fans Control</h1>

<p align="center">
  <strong>Terminal dashboard and fan control utility for Raspberry Pi 5.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Raspberry%20Pi%205-00d084?style=for-the-badge" alt="Platform" />
  <img src="https://img.shields.io/badge/C%2B%2B-14-00c2ff?style=for-the-badge" alt="Language" />
  <img src="https://img.shields.io/badge/UI-Terminal-7cf03a?style=for-the-badge" alt="Interface" />
  <img src="https://img.shields.io/badge/Target-Linux%20ARM64-ffb703?style=for-the-badge" alt="Target" />
</p>

<p align="center">
  RPI5 FUN CTRL gives you a fast SSH-friendly monitor for CPU temperature, CPU speed, per-core load, GPU load, RAM, load average and fan state, plus direct manual PWM control, logs and auto/manual switching in one terminal app.
</p>

## **Download link for linux**

```bash
git clone https://github.com/DeziXsteroid/Raspberry-Fans-Control.git && cd Raspberry-Fans-Control && chmod +x run.sh && ./run.sh
```

## **Project Preview**

<p align="center">
  <img src="assets/screenshots/auto-check.png" alt="Auto Check" width="900" />
</p>

## **Why This Project Is Cool**

> **Live terminal UI** that works well directly on Raspberry Pi and over SSH
>
> **Manual fan control** with fixed PWM hold
>
> **Auto fan mode** with proper manual release
>
> **Built-in logs screen** for fan actions and live telemetry
>
> **Runtime settings** for Linux paths right inside the app
>
> **One-command startup** through `run.sh`
>
> **Clean repository** with source code, docs and screenshots only

## **Quick Guide**

<details>
<summary><strong>Build manually</strong></summary>

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
sudo ./build/Raspberry_Fun_Control
```

</details>

<details>
<summary><strong>Main menu</strong></summary>

- `1` Start Auto Check
- `2` Fans Control
- `3` Logs
- `4` Settings
- `5` Help And Info
- `6` Exit

</details>

<details>
<summary><strong>Auto Check controls</strong></summary>

- `Q` or `Й` stop live monitoring

</details>

<details>
<summary><strong>Fans Control keys</strong></summary>

- `1` full speed
- `2` fan off
- `3` set custom PWM value
- `4` auto mode
- `0` back

</details>

## **Features**

> **CPU monitoring** with temperature, frequency and total load
>
> **Per-core stats** for Raspberry Pi 5 cores
>
> **GPU usage** support
>
> **RAM usage** with used and total memory display
>
> **Load average** in the footer
>
> **Fan status** with RPM, PWM and current mode
>
> **Built-in log viewer** with auto reset after 1 MB
>
> **Live history strip** with low, medium and high visual levels
>
> **Runtime settings editor** for Linux paths

## **Requirements**

- Raspberry Pi 5
- Debian or Raspberry Pi OS 64-bit
- `cmake` 3.10+
- C++14 compiler
- `sudo` access for fan control operations

## **License**

This repository includes the MIT License.
