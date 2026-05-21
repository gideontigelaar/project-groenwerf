# Building the firmware (Mac)

## Prerequisites

The easiest way is to install the Raspberry Pi Pico VS Code extension, which downloads the SDK and toolchain automatically into `~/.pico-sdk/`. You can then build via the extension or the terminal.

For terminal builds you also need CMake:
```bash
xcode-select --install
brew install cmake
```

Set the SDK path before building:
```bash
export PICO_SDK_PATH=~/.pico-sdk/sdk/2.2.0
```

## Credentials

Copy the template and fill in your WiFi, server details, and API key:
```bash
cp include/credentials.h.template include/credentials.h
```

Generate a key with:
```bash
openssl rand -hex 32
```

The same key goes in both `firmware/include/credentials.h` and `server/credentials.py`.

## Clean build

Do this the first time, or after changing `CMakeLists.txt`. In `firmware/`:
```bash
rm -rf build && mkdir build && cd build
cmake ..
make -j4
```

## Recompiling

In `firmware/`:
```bash
cd build && make -j4
```

## Flashing

Hold BOOTSEL, plug in USB and the Pico shows up as a drive called `RP2350`. In `firmware/build/`:
```bash
cp -X grass_monitor_pico.uf2 /Volumes/RP2350/
```

## Viewing output

```bash
screen $(ls /dev/tty.usbmodem*) 115200
```
Exit with `Ctrl+A` then `K`.