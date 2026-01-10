# LED Grid Simulator

A desktop simulator that compiles and runs the actual Arduino C++ code with mock libraries.

## Prerequisites

- CMake 3.10+
- SDL2 (`brew install sdl2`)

## Build

```bash
cd simulator
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./led_simulator
```

## Controls

| Key | Action |
|-----|--------|
| `←` / `→` | Previous / Next image |
| `-` / `+` | Decrease / Increase brightness |
| `Space` | Play / Pause |
| `0-9` | Direct image selection |
| `Q` | Quit |

## Options

```bash
./led_simulator --scale 10     # Larger display
./led_simulator --no-gap       # No gap between LEDs
./led_simulator --help         # Show all options
```
