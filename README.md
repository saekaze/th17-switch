# Touhou 17: Wily Beast and Weakest Creature — Nintendo Switch Port
*(東方鬼形獣　〜 Wily Beast and Weakest Creature)*

![Platform](https://img.shields.io/badge/Platform-Nintendo%20Switch-e60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)
![Status](https://img.shields.io/badge/Status-Playable-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/Port%20Code-CC0%201.0-blue?style=for-the-badge)

A native homebrew port of ZUN's 2019 bullet hell **Touhou 17: Wily Beast and Weakest Creature** for the **Nintendo Switch** (Horizon OS).

This build runs the TH17 C++ runtime from [eagler-touhou](https://github.com/Ritosa2333/eagler-touhou) (`ports/th17`), compiled **natively for ARM64** (no wasm, no emulation) and driven by an SDL2 host on an OpenGL ES 3 context — no Linux, Box64 or Wine involved.

Companion to the [Touhou Switch ports](https://github.com/saekaze/touhou-switch), with the same `touhou17.nro` + `sd:/switch/touhou/touhou17/` layout.

---

## 🆕 First release (1.00b-r1)

* 🐺 **The full game:** Reimu, Marisa and Youmu with the wolf, otter and eagle spirits, all 6 stages and Extra, endings, staff roll and replays.
* 🧊 **No freezes from heavy frames.** The game runs exactly one tick per screen refresh. A slow frame never makes the next frames run extra catch-up ticks, so a spell capture or a screen full of bullets can't snowball into a freeze.
* ⚡ **Faster stage changes.** The next stage's files are read and decompressed on another CPU core while you play the current one, so the stage change frame is 38% shorter.
* 🔊 BGM streamed straight from `thbgm.dat` with the original loop points.

---

## ✨ Key Features

* ⬛ **OLED-Friendly Pillarboxing:** the original 640×480 picture is scaled to the screen height with pure black bars.
* 🔊 **Full Audio:** sound effects plus BGM streamed out of your `thbgm.dat` (loop points from the game's own `thbgm.fmt` — no conversion step).
* 🎮 **Sane, Remappable Controls:** Joy-Con (handheld, grip, detached) and Pro Controller with the same default layout as every other Touhou Switch port (B shoot, A bomb, L/ZL focus, R/ZR skip, + pause); the in-game **Key Config** can rebind them.
* 👆 **Touch Screen:** in handheld mode, drag to move and tap menus — upstream's touch controller, mapped through the pillarbox.
* 🈂️ **Japanese Text Without Windows:** text is drawn on demand from `msgothic.ttc`, which is read once and kept in memory. No font building step, no long first launch.
* 🌏 **Language-Aware Title:** hbmenu shows `東方鬼形獣　～ Wily Beast and Weakest Creature` on consoles set to 日本語 and the romanised title everywhere else.
* 💾 **Saves Next to the Data:** `th17.cfg`, `scoreth17.dat`, replays (`replay/`) and snapshots are written into the SD folder the game loaded from, like the PC release.

---

## 📥 Installation Guide

> ⚠️ **Disclaimer:** In compliance with ZUN's guidelines and copyright law, this repository contains **ONLY the homebrew engine code**. No game assets are distributed. You must legally own a copy of *Touhou 17: Wily Beast and Weakest Creature v1.00b*.

### 1. SD Card File Structure

1. Ensure your Nintendo Switch is running custom firmware (Atmosphère CFW).
2. Download the latest `touhou17.nro` from the [Releases](../../releases) tab (or build from source).
3. Create the folder `sd:/switch/touhou/touhou17/` and copy the following into it:

```text
sd:/switch/touhou/touhou17/
    ├── touhou17.nro          # Nintendo Switch homebrew executable
    ├── th17.dat              # Main game archive (v1.00b)
    ├── thbgm.dat             # Background music archive
    └── msgothic.ttc          # MS Gothic (C:\Windows\Fonts) - menu and dialogue text
```

`thbgm.dat` is optional (the game runs silent without it).

**Recommended place: `sd:/switch/touhou/touhou17/`.** Keeping every Touhou port in one `sd:/switch/touhou/` folder (`touhou6`, `touhou7`, `touhou8` …) is much tidier than a separate folder per game. Other places still work: the port first looks in its own folder (wherever the NRO is), then for a `th17` / `touhou17` folder (any capitalisation) directly on the SD card, in `switch/`, `touhou/`, `switch/touhou/`, `games/` or `roms/`.

> 💡 TH17 keeps its textures in memory (about 150 MB by stage 2, plus the GPU copies). If it runs out of memory from the album, launch hbmenu through a game (hold **R** while starting any game) for full RAM.

### 2. Launching

Run `touhou17.nro` from the **Homebrew Menu (hbmenu)**, **Sphaira launcher**, or a home screen forwarder. Title-menu **Quit** returns to hbmenu. If `th17.dat` is missing, the port says so and shows the folder it used. `th17-switch.log` is written next to the saves: it records start-up, memory use at every stage and any frame that took longer than 25 ms.

---

## 🕹 Controls

| Nintendo Switch Button | Action |
| :--- | :--- |
| **Left Stick / D-Pad** | Character Movement |
| **B** | Shoot / Confirm |
| **A** | Bomb / Cancel |
| **L / ZL** | Focus (Precision Slow-Motion Movement) |
| **R / ZR** | Skip Dialogue (hold, Ctrl) |
| **+ (Plus)** | Pause / In-Game Menu |
| **X** | Enter (while not bound in Key Config) |
| **Y** | R key (Retry shortcut, while not bound in Key Config) |
| **− (Minus)** | Snapshot (saved to `snapshot/` next to the game data) |
| **Touch** (handheld) | Drag to move, tap menus |

**The same default layout in every Touhou Switch port:** B shoots, A bombs, L/ZL focuses, R/ZR skips dialogue, + pauses. These are only defaults — the Switch buttons act as the game's own gamepad, so the in-game **Option → Key Config** can rebind shot, bomb, focus and pause. The D-Pad and sticks only move — they can never be picked as a button. Key Config numbers: 0 B, 1 A, 2 L/ZL, 3 R/ZR, 4 +, 5 X, 6 Y.

---

## 🛠 Building from Source

### Automated Build (GitHub Actions)

Every push builds `touhou17.nro` in the `devkitpro/devkita64` container (`.github/workflows/build-switch.yml`). Tags (`v*`) also publish it as a release with a SHA-256 checksum.

### Local Build (Linux / macOS / WSL)

The TH17 runtime is not in this repository: `scripts/fetch_upstream.sh` downloads it from eagler-touhou at the commit pinned in `UPSTREAM_COMMIT` (only `ports/th17/web/cpp`, `ports/th17/portable` and the shared `common/include` headers) and applies `patches/native.patch` on top.

Switch (devkitPro with devkitA64, libnx, `switch-sdl2`, `switch-sdl2_ttf` and `switch-mesa`):

```sh
./scripts/fetch_upstream.sh
./scripts/build_switch.sh        # build-switch/touhou17.nro
```

Linux test build (SDL2, SDL2_ttf and GLES3 development packages):

```sh
./scripts/fetch_upstream.sh
cmake -S . -B build -G Ninja && ninja -C build
./build/touhou17 /path/to/game/folder
```

The Linux build can play itself for testing: `TH17_SCRIPT` (a file of `frame mask` lines of held game bits), `TH17_FRAMES`, `TH17_NOVSYNC`, `TH17_SNAP_EVERY`, `TH17_TEST_TOOLS` (practice tool bits, e.g. `1` for invincibility), `TH17_TEST_STAGE` (e.g. `7`: Game Start plays the Extra stage) and `TH17_LOGICAL` (logic only, no GPU draws; a frame number: drawn from that frame on).

---

## 📂 How It Works

* **One tick per vsync:** `src/main.cpp` runs one game tick per 60 Hz refresh and reads the controller and touch screen with libnx (it replaces the browser runtime).
* **SDL3 on SDL2:** the runtime is written against SDL3 and SDL3_ttf; `compat/` maps that API onto the SDL2 and SDL2_ttf that devkitPro ships, and stands in for `<emscripten.h>`.
* **Next-stage prefetch:** `src/ArchivePrefetch.cpp` reads and decompresses the next stage's `th17.dat` entries on a worker thread, so the stage change itself only prepares textures.
* **Music:** `src/BgmStream.cpp` plays `thbgm.dat` tracks as a miniaudio data source.
* **Files and text:** `src/Platform.cpp` finds the data folder, maps the game's paths onto it, keeps `msgothic.ttc` in memory and carries the CP932 table for Japanese text.
* **`patches/native.patch`** is everything the native build changes upstream: the window and letterboxing, music from `thbgm.dat`, save paths, FAT-safe file replacement, the next-stage prefetch hook, a GLSL name Mesa rejects, a GCC-only aggregate assignment, and the native vertex-stream offsets of the staged renderer (without that fix the gameplay screen was black).

---

## 🤝 Credits & Acknowledgments

* **ZUN / Team Shanghai Alice** — original creator and developer of the Touhou Project series.
* **[eagler-touhou](https://github.com/Ritosa2333/eagler-touhou)** (Ritosa2333 and contributors) — the TH17 C++ runtime this port is built on.
* **Switchbrew & devkitPro Team** — the open-source `libnx` SDK and Switch toolchain.

**Licensing:** the Switch port code in this repository (`src/`, `compat/`, `scripts/`, build files) is CC0 1.0 (see `LICENSE`). The TH17 runtime is not part of this repository; it is downloaded from eagler-touhou at build time and stays under its authors' terms, as do the third-party libraries it bundles (see `ports/th17/THIRD-PARTY-NOTICES.txt` in the fetched sources).
