# Firmware Documentation (Mac)

This directory contains the embedded code for the Raspberry Pi Pico 2W.

## Building and Deployment
1. **Toolchain**: Ensure the Raspberry Pi Pico SDK is installed.
2. **Credentials**:
   - Copy the template: `cp include/credentials.h.template include/credentials.h`
   - Edit the file: `nano include/credentials.h`
   - Ensure the `API_KEY` matches the value configured in your `api/` deployment.
3. **Build**:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j4
   ```
4. **Flash**: Hold the `BOOTSEL` button while plugging in the Pico, then copy the resulting `.uf2` file to the RPI-RP2 drive.

## Configuration
See `api/README.md` for instructions on setting up the backend endpoint that the firmware expects.