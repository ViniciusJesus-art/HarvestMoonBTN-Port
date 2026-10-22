<div align="center">

# Harvest Moon: Back to Nature — PC Port

**A personal project to port Harvest Moon: Back to Nature (PlayStation) natively to Linux, Windows, and Android.**

*Based on the [hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp) decompilation project by coalslav.*

> **Status: title, registration, farm arrival, and childhood sequence (partial M3/M4).** Walking and running inside the initial house are available; farm gameplay remains pending. Current development focuses on Linux and Windows; Android is planned for the future.

**Veja a Versão em Português 🇧🇷 [aqui](/README-PT-BR.md)**

---

## About the Project

The goal is to study and adapt the reconstructed C code of **Harvest Moon: Back to Nature** so it can run on modern platforms without requiring a PlayStation emulator at runtime. I aim to preserve the original game's experience and, eventually, explore improvements and mod support.

This is a personal project driven by my interest in programming, reverse engineering, and the game itself. This port is unofficial and is not affiliated with the Harvest Moon rights holders or the authors of the decompilation project.

</div>

## Table of Contents

- [Current Status](#current-status)
- [Getting Started on Linux and Windows](#getting-started-on-linux-and-windows)
- [Goals](#goals)
- [Planned Architecture](#planned-architecture)
- [Platforms](#platforms)
- [Documentation](#documentation)
- [Building the Original Decompilation](#building-the-original-decompilation)
- [Origins and Credits](#origins-and-credits)
- [Licensing and Game Data](#licensing-and-game-data)

## Current Status

The repository contains the **decompilation of the US PlayStation release, `SLUS_011.15`**. The upstream project reconstructs the executable and its 13 overlays with byte-for-byte matching output.

**Implemented:** an SDL3 window with a logical resolution of 320 × 240, the Natsume logo, the gradual title reveal, animated grass and sprites, and states 0–9 of the original menu adapted to C. New Game already displays dialogues and portraits and allows the player to complete name entry, birthday selection, and the registration summary. Music and button sound effects use VAB/SEQ data from the extracted game assets, with audio output through SDL3. The game viewport has no diagnostic bars or text. Enter/Start skips the introduction; pressing Enter again opens the menu. Arrow keys/D-pad navigate, and Z/the bottom face button confirms. Escape quits, and F11 toggles fullscreen. Linux has been validated; a Windows x64 build is available, and MSVC CI is set up. The build presets compile and test one task at a time.

**Game data:** the executable reads the title-screen data block from `data/A_FILE/00_gf.bin` without accessing the original disc. Folder import/verification and PS1 reconstruction from the extracted data are available. See the [game data guide](docs/port/GAME_DATA.md) and [implemented asset formats](docs/port/ASSET_FORMATS.md).

**3D scene — partial M4:** `./build/native-debug/port/hmbtn_native --scene-preview` displays the base farm model using original textures, with native C projection and rasterization. Arrow keys move the diagnostic camera, and Enter restores the default view. This is a preview without characters or gameplay, separate from the New Game flow. See [SCENE_RENDERER.md](docs/port/SCENE_RENDERER.md).

**Farm arrival:** accepting the registration now starts the original arrival script, with the player, Thomas, dialogue, camera movement, and music. `./build/native-debug/port/hmbtn_native --intro-preview` opens this sequence directly. The scenery includes doors, the shipping box, weeds, stones, and branches. It continues through the childhood meeting and song, the farewell, and the return to Thomas (events `0x75..0x79`). The sequence now enters the farmhouse (area 80), places the player at the door and finishes the opening script. `--childhood-preview` starts directly at the meeting. See [FARM_INTRO.md](docs/port/FARM_INTRO.md).

**House controls:** `./build/native-debug/port/hmbtn_native --house-preview` opens the initial house directly. Arrows/D-pad walk; X/Circle runs forward. Ground queries use the original scene polygons and flags. Reaching the exit pauses at an explicit boundary; the farm transition and furniture interactions are pending. See [HOUSE_CONTROL.md](docs/port/HOUSE_CONTROL.md).

**Not yet implemented:** full gameplay, saves, general collisions, dynamic farm objects, full SPU audio/reverb, mods, or Android. The bootstrap does not run the complete game. Continue/demo stop at a boundary reported in the terminal, and Start/Z/X returns to the menu. The opening sequence and audio still need comparison with the PS1 version before full fidelity can be claimed. This portion is adapted under `port/`; the complete modules in `src/` are not compiled for the host platform. The original **PlayStation** build remains available by default. The local Windows `.exe` was rebuilt with MinGW x64 including the childhood sequence and streamed XA audio event; testing on actual Windows/MSVC is pending.

## Getting Started on Linux and Windows

Requirements: CMake 3.20+, a C11 compiler, Ninja, and SDL3 development files. If SDL3 is not installed, CMake downloads a pinned version and verifies its hash; the first build therefore requires an internet connection. On Windows, run the commands in a Visual Studio Developer PowerShell with the C++ tools and Ninja available. See the [build guide](docs/port/BUILDING.md) for offline builds and builds without Ninja.

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Run `./build/native-debug/port/hmbtn_native` on Linux or `.\build\native-debug\port\hmbtn_native.exe` on Windows. The program looks for `data/` next to the executable and in the project layout; `--data DIR` takes precedence. Enter opens the menu, the arrow keys move the selection, and Enter/Z confirms. X/Backspace returns from the notice indicating that the selected destination has not yet been ported. `--window-test` opens the diagnostic window without game data, and `--diagnostics` runs console-only checks without a display. See also [Windows and cross-compilation](docs/port/WINDOWS.md).

## Goals

- Port the game's logic to a native application for Linux, Windows, and Android.
- Create a compatibility layer for PS1 graphics, audio, input, timing, memory, and file access.
- Keep the decompiled code separate from cross-platform adaptations as much as possible.
- Preserve a way to track updates from the original decompilation project.
- Consider mod support and enhancements only after the base game is working.

## Planned Architecture

| Layer | Responsibility | Status |
|---|---|---|
| `src/` | Decompiled game logic | Existing codebase |
| `port/core/` | Host application and PS1 data formats | Buildable foundation |
| `port/game/` | Adapted title flow and event commands | Menu, registration, and first farm event |
| `port/tests/` | Asset formats, import, window, and event tests | Implemented |
| `port/graphics/` | Banks, animations, TMD models, and projected sprites | Title and farm arrival implemented in part |
| `port/audio/` | VAB/ADPCM sampler and title SEQ sequencer | Partially implemented; reverb pending |
| `port/input/` | Keyboard shortcuts and mapping to PS1 buttons | Keyboard/gamepad implemented |
| `port/filesystem/` | Resource-range reads; future save support | Reading implemented |
| `port/platform/` | SDL3 window/renderer | Implemented |

**SDL3 is integrated for window management, events, and presentation.** Translating PS1 graphics and adapting the game logic still require further work on PSY-Q, fixed memory addresses, overlays, and console-specific data structures. See the milestones in the [roadmap](docs/port/ROADMAP.md).

The original build remains organized around its existing `CMakeLists.txt` files and PS1 toolchain. The native build uses `-DHMBTN_BUILD_NATIVE=ON` and a separate build directory, selected before the PS1 toolchain. Without this option, the PS1 build flow remains the default.

## Platforms

| Platform | Goal | Status |
|---|---|---|
| Fedora / Linux | Primary development platform | Window validated on Wayland and X11 |
| Windows | Native PC build | MinGW x64 and Wine verified; testing on actual Windows/MSVC pending |
| Android | Port using the NDK, Android lifecycle, and touch controls | Not started |

## Documentation

- [Linux and Windows build instructions](docs/port/BUILDING.md)
- [Windows build and portable package](docs/port/WINDOWS.md)
- [Adapted title flow](docs/port/TITLE_FLOW.md)
- [Initial PS1 code audit](docs/port/INITIAL_AUDIT.md)
- [Technical development roadmap](docs/port/ROADMAP.md)
- [Initial architecture](docs/port/ARCHITECTURE.md)
- [Contribution guidelines and upstream synchronization](CONTRIBUTING.md)
- [Original decompilation project](https://github.com/coalslav/hmbtn-decomp)

## Building the Original Decompilation

The command below **builds for PlayStation 1**, not Linux or Windows. It requires Docker and the tools installed by the upstream project's image:

```bash
docker build -t hmbtn-build .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src:ro -v /tmp/hmbtn-build:/build \
  hmbtn-build bash -c 'cmake -S /src -B /build -G Ninja && cmake --build /build --target check'
```

The `check` target verifies the hashes of the 14 original binaries. **Once the port or gameplay modifications begin, the original hashes will no longer be valid checks for modified binaries.** Rebuilding the disc image through the original workflow requires your own copy of the disc; refer to the upstream documentation.

## Origins and Credits

This repository is a **fork** of [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp). The decompilation and its initial structure were created by the upstream project. Development of the port will take place separately, and the original authorship history will be preserved.

Thanks to the decompilation contributors, the creators of the tools used, and the developers of the original game.

## Licensing and Game Data

The upstream project's [CC0 1.0](LICENSE) license covers **only the original repository's own contributions**. Components such as `tools/maspsx` may have their own licenses. The game, its graphics, music, assets, and the PSY-Q SDK belong to their respective rights holders and are not released under CC0.

The port must not distribute copies of the game or proprietary game data without authorization. The intended approach is to investigate a process in which each user supplies assets from their own copy of the game. Public distribution of a port containing such assets requires an assessment of the applicable rights.
