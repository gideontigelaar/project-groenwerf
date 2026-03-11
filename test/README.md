# Building the project
You'll need CMake, the ARM toolchain (`arm-none-eabi-gcc`), and the Pico SDK installed.

Create a `.env` file in the project root and set `PICO_SDK_PATH` to your SDK folder:
```dotenv
PICO_SDK_PATH=/path/to/your/.pico-sdk/sdk/2.2.0
```

Before building, load the variable into your environment:

**Mac/Linux:** `export $(cat .env | xargs)`<br>
**Windows (PowerShell):** `$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"`

`CMakeLists.txt` picks this up via `$ENV{PICO_SDK_PATH}` to locate the SDK.

## Clean build
Do this the first time, or when you've changed `CMakeLists.txt`:
```bash
rm -rf build && mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j4
```

Windows: use `rmdir /s /q build` and `mingw32-make -j4`

## Just recompiling
```bash
cd build
make -j4
```

## Flashing
Hold BOOTSEL, plug in, drop the `.uf2` onto the drive.

**Mac:** `cp -X vl53l1x_pico.uf2 /Volumes/RP2350/`<br>
**Windows:** drag `build/vl53l1x_pico.uf2` onto the RP2350 drive

## Viewing output
**Mac:** `screen $(ls /dev/tty.usbmodem*) 115200`<br>
**Windows:** PuTTY → Serial → your COM port (check Device Manager) → 115200 baud