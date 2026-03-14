# Building the project (Mac)

## Prerequisites
You need the Pico SDK and ARM toolchain installed. The easiest way is to install the Raspberry Pi Pico VS Code extension, which will download and install everything into `~/.pico-sdk/`. You don't need to use the extension to build, it just handles the installation for you.

You also need Xcode Command Line Tools, CMake, and the ARM toolchain:
```bash
xcode-select --install
brew install cmake
brew install --cask gcc-arm-embedded
```

If you already have the full Xcode app installed, skip the `xcode-select` step.

Once done, set `PICO_SDK_PATH` in your environment before building:
```bash
export PICO_SDK_PATH=~/.pico-sdk/sdk/2.2.0
```

`CMakeLists.txt` reads this variable via `$ENV{PICO_SDK_PATH}` to find the SDK.

## Clean build
Do this the first time, or after changing `CMakeLists.txt`:
```bash
cd firmware
rm -rf build && mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j4
```

## Recompiling
```bash
cd firmware/build && make -j4
```

## Flashing
Hold BOOTSEL, plug in USB and the Pico shows up as a drive called `RP2350`.
```bash
cp -X firmware/build/grass_monitor_pico.uf2 /Volumes/RP2350/
```

## Viewing output
```bash
screen $(ls /dev/tty.usbmodem*) 115200
```
Exit with `Ctrl+A` then `K`.