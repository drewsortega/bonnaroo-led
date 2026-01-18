Materials:

Teensy 4.0 https://www.adafruit.com/product/4323?srsltid=AfmBOopSE8MFLm7p8g6QQ8bGNTT_F5kXwXLrUOeNOoeIDeXyiCY26EuX

SmartShield matrix https://docs.pixelmatix.com/SmartMatrix/shieldref.html

SDCard Reader https://a.co/d/3aymKGg

Soldering iron for headers to teensy

More details TBA (after Bonnaroo!)

## Simulator

This project includes a fully functional desktop simulator allowing you to run and debug the LED grid code on your computer (macOS/Linux) without the hardware.

### Features
- **Visualizes the LED Grid**: Uses SDL2 to render the 64x64 matrix on your screen.
- **Compiles Real Code**: Compiles `Bonnaroo.ino` directly, mocking the hardware interfaces (`SD`, `IRRemote`).
- **Vendored Libraries**: All core libraries are vendored in `src/`:
  - `src/SmartMatrix/` - LED matrix driver library (patched for simulator compatibility)
  - `src/AnimatedGIF/` - GIF decoder library
  - `src/GifDecoder/` - GIF decoder wrapper
- **Same Code for Hardware and Simulator**: Uses `#ifdef SIMULATOR_MODE` to switch includes between mocked and real hardware headers.
- **GIF Playback**: Reads GIFs from the local `gifs/` directory, identical to SD card behavior.

### Controls
The simulator maps keyboard keys to the IR remote functions:

- **Arrow Up / Down**: Increase / Decrease Brightness
- **Arrow Left / Right**: Previous / Next GIF
- **Space**: Play / Pause
- **Q**: Quit

### Building and Running

**Prerequisites:**
- `cmake`
- `sdl2` (install via `brew install sdl2 sdl2_ttf` on macOS)

**Run Command:**
From the project root:
```bash
make run-simulator
```

This will compile the simulator (using the local SmartMatrix library) and launch the window.
