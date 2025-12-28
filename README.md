# Nacfetch

**A fast, modern C++ system information tool with ASCII logos**

Nacfetch is a high-performance, modern C++ system information fetcher inspired by tools like *neofetch* and *fastfetch*.
It focuses on **speed**, **clean output**, and **extensive distro logo support** (200+ ASCII logos).

Built with modern C++, designed for Linux and Windows, and optimized to hell and back.

---

## ✨ Features

* 🖥️ OS & distribution detection
* ⚙️ Kernel information
* 💻 Hardware / model detection
* 🔧 CPU information
* 🎮 GPU detection (multi-GPU supported)
* 💾 RAM usage + percentage + progress bar
* 🖼️ Screen resolution
* ⏱️ System uptime
* 🐚 Shell detection
* 🎨 Colored output (256-color terminals)
* 🖼️ **200+ embedded ASCII distro logos**
* ⚡ Ultra-fast builds (Intel / AMD tuned binaries)

---

## 🚀 Why Nacfetch?

* **Modern C++ (C++20)**
* **RAII & STL only** — no malloc/free nonsense
* **Cross-platform** (Linux + Windows)
* **Highly optimized builds** (LTO, fast-math, CPU-specific)
* **Extremely extensible logo system**

---

## 📦 Supported Platforms

| OS                     | Status            |
| ---------------------- | ----------------- |
| Linux                  | ✅ Fully supported |
| Windows (MinGW / MSVC) | ✅ Supported       |
| macOS                  | ❌ Not yet         |

---

## 🧱 Project Structure

```
.
├── src/
│   ├── sysinfo.cpp          # Linux implementation
│   ├── sysinfo.win.cpp      # Windows implementation
│   ├── sysinfo.hpp
│   ├── sysinfo.win.hpp
│   └── main.cpp
├── build-win.sh             # MinGW Windows build
├── CMakeLists.txt
├── README.md
└── output/                  # Build artifacts
```

---

## 🛠️ Building

### Requirements

* C++20 compiler
* CMake ≥ 3.16
* Linux:

  * `base-devel`
  * `cmake`
* Windows cross-build:

  * `mingw-w64`

---

### Linux Build

```bash
cmake -S . -B build
cmake --build build
```

Binaries are generated in:

```
./output/
```

---

### Windows (cross-compiled from Linux)

```bash
chmod +x build-win.sh
./build-win.sh
```

Produces `.exe` files in `./output`.

---

## 📂 Output Binaries

```
output/
├── nacfetch-linux-amd
├── nacfetch-linux-generic
├── nacfetch-linux-intel
├── nacfetch-windows-amd.exe
├── nacfetch-windows-generic.exe
└── nacfetch-windows-intel.exe
```

Each binary is **CPU-tuned** for maximum performance.

---

## 🧪 Usage

```bash
./nacfetch-generic
```

### Command-line Options

| Option           | Description              |
| ---------------- | ------------------------ |
| `--help`         | Show help                |
| `--minimal`      | Minimal output (no logo) |
| `--no-gpu`       | Skip GPU detection       |
| `--no-packages`  | Skip package counting    |
| `--logos <path>` | Custom logo JSON file    |

---

## 🖼️ Logo System

### How it works

1. Detects OS / distro name
2. Matches it against `distroMapping`
3. Resolves a **logo index**
4. Applies distro-specific color
5. Prints cleanly in terminal

---

## 🎨 Color Palette

Uses 256-color ANSI codes for soft pastel output:

* Pink, lavender, mint, peach, cyan, lilac, rose
* Automatically disabled on unsupported terminals

---

## 🧠 Performance

* Direct file reads (`/proc`, `/etc/os-release`)
* Minimal external command usage
* Optional feature skipping
* LTO + fast-math + CPU-specific builds

Nacfetch launches **noticeably faster** than traditional fetch tools.

```sh
[nacreousdawn596@Me:~/Documents/nacfetch]$ hyperfine ./output/nacfetch-intel
Benchmark 1: ./output/nacfetch-intel
  Time (mean ± σ):       3.3 ms ±   1.6 ms    [User: 1.1 ms, System: 1.9 ms]
  Range (min … max):     1.8 ms …  17.7 ms    149 runs

[nacreousdawn596@Me:~/Documents/nacfetch]$ hyperfine fastfetch
Benchmark 1: fastfetch
  Time (mean ± σ):     116.0 ms ±  31.5 ms    [User: 50.9 ms, System: 42.5 ms]
  Range (min … max):   103.3 ms … 219.9 ms    13 runs
```

---

## 🧪 Example Output
```
╭─────────────────────────────────────────────╮
│  ✨ S Y S T E M   I N F O R M A T I O N ✨  │
╰─────────────────────────────────────────────╯

 ____^____
 |\\  |  /|
 | \\ | / |
<---- ---->
 | / | \\ |
 |/__|__\\|
     v
👤 User ········· nacreousdawn596@Me
🖥️  OS ············ NixOS x86_64
💻 Host ········· ThinkPad E14 Gen 2
⚙️  Kernel ······· Linux 6.14.7-zen1
⏱️  Uptime ······· 8h 01m
🐚 Shell ········ bash
🖼️  Display ······ 1920×1080 [Built-in]
🎨 DE ············ GNOME
💻 Terminal ····· vscode

─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─

🔧 CPU ············ 11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz (1) @ 4.20 GHz
🎮 GPU ············ 0x9a49 [Integrated]
💾 Memory ······· 6.97 GiB / 15.3 GiB (45%)
                  [●●●●●●●●●○○○○○○○○○○○]
💿 Swap ·········· 7.12 GiB / 8.00 GiB (89%)
                  [●●●●●●●●●●●●●●●●●○○○]
💾 Disk (/)       337.7 GiB / 363.9 GiB (92%) - ext4
💾 Disk (/nix/store) 337.7 GiB / 363.9 GiB (92%) - ext4
💾 Disk (/boot)   171.1 MiB / 512.0 MiB (33%) - vfat
💾 Disk (/tmp/com.freerdp.client.cliprdr.78567) 0.0 B / 0.0 B (0%) - fuse

─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─

🌐 Network (br-45e3a33e3a6a) 
🌐 Network (br-d523738a1b10) 
🌐 Network (docker0) 
🌐 Network (br-cdc9e88219c8) 
🌐 Network (enp4s0) 
🌐 Network (veth70825ee) 
🌐 Network (wlp0s20f3)  [Wireless]
🌐 Network (veth3641a07) 
🔋 Battery (BAT0) 79% [Not charging]
                  [●●●●●●●●●●●●●●●○○○○○]
🌍 Locale ······· en_US.UTF-8

╭─────────────────────────────────────────────────╮
│  ✧･ﾟ: *✧･ﾟ:* Have a wonderful day! *:･ﾟ✧*: ･ﾟ✧  │
╰─────────────────────────────────────────────────╯

```

---

## 📜 License

GPL-3.0
Based on ideas from UwUfetch / Neofetch, rewritten entirely in modern C++.

---

## 🤝 Contributing

PRs welcome for:

* New distro logos
* Better OS detection
* Performance improvements
* macOS support
* New output formats (JSON, YAML)

---

## 🧠 Philosophy

> **Fast. Clean. No bullshit.**

If it slows startup, it’s optional.
If it’s ugly, it gets deleted.
If it’s unsafe, C++ will complain.

---
