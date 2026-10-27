<div align="center">

# Harvest Moon: Back to Nature — PC Port

**A personal project to port Harvest Moon: Back to Nature (PlayStation) natively to Linux and Windows.**

*Based on the [hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp) decompilation project by coalslav.*

> **Status: playable partial port — title, opening, farmhouse, farm, village, farming and native saves.** Core systems are progressing, but this is not yet the complete game.

**Veja a Versão em Português 🇧🇷 [aqui](/README-PT-BR.md)**

---

<img src="./docs/prints/animacao_estavel.gif" width="300">

***PC port in progress...***

## PC installers

AppImage, DEB, RPM and EXE packages contain only the native port and its resource importer. Select your `.bin` on first launch; only resources are extracted into the user data folder. No Python, SDK or emulator is required. See [installation and GitHub Actions](docs/port/INSTALLERS.md). The full game is not yet ported.

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

This repository includes the matching decompilation of the US PlayStation version (`SLUS_011.15`) and a **separate, native C11/SDL3 port** for Linux and Windows. The original project's PS1 executable and 13 overlays are retained. The PC runtime uses adapted logic in `port/`; it does **not** compile the original `src/` game modules directly or emulate the PS1.

### Available in the current playable portion

- **Presentation and assets:** 320 × 240 logical viewport, original title sequences, sprite animations, textures/TMD scenery, native scene projection/rasterization, camera, SDL3 input, and partially implemented VAB/SEQ, sound effects and XA playback.
- **Opening and exploration:** New Game registration, farm arrival and childhood scenes, Thomas's first tour and Zack's introduction, house/farm/village exploration, connected outdoor regions, doors, walking/running, terrain height, interactions and selected scripted events.
- **Clock and daily cycle:** in-game time, calendar/seasons, day/night lighting, sleep and daily updates, with weather and rain implemented for the current gameplay scope.
- **Farming and economy:** shared inventory/tools, shops and seed purchasing, tilled soil, planting, watering, growth, harvest, shipping and payments. Turnip, potato, tomato, corn, onion, eggplant, carrot and sweet potato have tested purchase-to-shipping flows. Grass supports planting, cutting, regrowth and fodder storage. Won's seasonal seeds (cabbage, pineapple and green pepper) have also been integrated.
- **Other partial systems:** chickens/eggs/incubation and feed, villagers and schedules, conversations, gifts/affection, fishing, house interactions and selected original events.
- **Native save/load:** versioned local saves for stable free-exploration states, including Continue and diary loading. Ongoing events and transitions cannot be saved mid-action; PS1 memory-card saves are not imported.
- **Packaging:** AppImage, DEB, RPM and Windows EXE workflows. The user supplies their own game BIN on first launch; proprietary game assets are not shipped.

### Development and validation

The current roadmap records **MA-01–06 and PF-01–05 completed; PF-06 in progress**, with more original gameplay systems still to port. The most recent documented Linux Debug run passed **109/109 CTests** (including optional tests with locally installed user assets). This does not imply Windows hardware validation or complete PS1 fidelity. See the [live status](docs/port/ROADMAP_STATUS.md), [roadmap](docs/port/ROADMAP.md) and [feature matrix](docs/port/PORTING_MATRIX.md).

**Not complete yet:** the full story/year progression, all events and festivals, all animal species, marriage/family, every shop/tool/area, full audiovisual parity, mod support and Android. Some features above work only in the tested subset. This is **not a finished or fully playable recreation of the entire game**.

Diagnostic entry points include `--scene-preview`, `--intro-preview`, `--childhood-preview`, `--house-preview` and `--diagnostics`. See [farm introduction](docs/port/FARM_INTRO.md), [world map](docs/port/WORLD_MAP.md), [crop systems](docs/port/PF02_CROP_CYCLE.md), and [native saves](docs/port/PF05_NATIVE_SAVES.md).


---

<br>

<img src="./docs/prints/harvest_animacao_sleep.gif" width="110" align="left">


### Getting Started on Linux and Windows

Requirements: CMake 3.20+, a C11 compiler, Ninja, and SDL3 development files. If SDL3 is not installed, CMake downloads a pinned version and verifies its hash; the first build therefore requires an internet connection. On Windows, run the commands in a Visual Studio Developer PowerShell with the C++ tools and Ninja available. See the [build guide](docs/port/BUILDING.md) for offline builds and builds without Ninja.


<br clear="all">

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Run `./build/native-debug/port/hmbtn_native` on Linux or `.\build\native-debug\port\hmbtn_native.exe` on Windows. The program looks for `data/` next to the executable and in the project layout; `--data DIR` takes precedence. Enter opens the menu, the arrow keys move the selection, and Enter/Z confirms. X/Backspace returns from the notice indicating that the selected destination has not yet been ported. `--window-test` opens the diagnostic window without game data, and `--diagnostics` runs console-only checks without a display. See also [Windows and cross-compilation](docs/port/WINDOWS.md).

## Goals

- Port the game's logic to a native application for Linux and Windows.
- Create a compatibility layer for PS1 graphics, audio, input, timing, memory, and file access.
- Keep the decompiled code separate from cross-platform adaptations as much as possible.
- Preserve a way to track updates from the original decompilation project.
- Consider mod support and enhancements only after the base game is working.

## Planned Architecture

| Layer | Responsibility | Status |
|---|---|---|
| `src/` | Decompiled game logic | Existing codebase |
| `port/core/` | Host application and PS1 data formats | Buildable foundation |
| `port/game/` | Game state, exploration, farming, events and saves | Multiple playable systems; progression incomplete |
| `port/tests/` | Asset formats, import, window, and event tests | Implemented |
| `port/graphics/` | Banks, animations, TMD models, and projected sprites | Title and farm arrival implemented in part |
| `port/audio/` | VAB/ADPCM sampler and title SEQ sequencer | Partially implemented; reverb pending |
| `port/input/` | Keyboard shortcuts and mapping to PS1 buttons | Keyboard/gamepad implemented |
| `port/filesystem/` | Resource reads and native save files | Implemented for current scope |
| `port/platform/` | SDL3 window/renderer | Implemented |

**SDL3 is integrated for window management, events, and presentation.** Translating PS1 graphics and adapting the game logic still require further work on PSY-Q, fixed memory addresses, overlays, and console-specific data structures. See the milestones in the [roadmap](docs/port/ROADMAP.md).

The original build remains organized around its existing `CMakeLists.txt` files and PS1 toolchain. The native build uses `-DHMBTN_BUILD_NATIVE=ON` and a separate build directory, selected before the PS1 toolchain. Without this option, the PS1 build flow remains the default.

## Platforms

| Platform | Goal | Status |
|---|---|---|
| Fedora / Linux | Primary development platform | Window validated on Wayland and X11 |
| Windows | Native PC build | MinGW x64 and Wine verified; testing on actual Windows/MSVC pending |

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


![HMBTN — captura do projeto 1](./docs/prints/Screenshot_20261024_233235.png)


![HMBTN — captura do projeto 1](./docs/prints/Screenshot_20261024_233706.png)



## Origins and Credits

This repository is a **fork** of [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp). The decompilation and its initial structure were created by the upstream project. Development of the port will take place separately, and the original authorship history will be preserved.

Thanks to the decompilation contributors, the creators of the tools used, and the developers of the original game.

## Licensing and Game Data

The upstream project's [CC0 1.0](LICENSE) license covers **only the original repository's own contributions**. Components such as `tools/maspsx` may have their own licenses. The game, its graphics, music, assets, and the PSY-Q SDK belong to their respective rights holders and are not released under CC0.

The port must not distribute copies of the game or proprietary game data without authorization. The intended approach is to investigate a process in which each user supplies assets from their own copy of the game. Public distribution of a port containing such assets requires an assessment of the applicable rights.

House music, footsteps, TV channels, calendar and toolbox interactions are described in [HOUSE_INTERACTIONS.md](docs/port/HOUSE_INTERACTIONS.md).
