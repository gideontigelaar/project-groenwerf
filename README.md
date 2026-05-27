# Project Groenwerf

A grass height measurement system for ride-on mowers, utilizing a Raspberry Pi Pico 2W to process sensor data and transmit it to a remote server.

## Project Structure
- **`firmware/`**: Embedded C++ source code for the Raspberry Pi Pico 2W (Mac/Dev). Handles sensor polling, data processing, and network transmission.
- **`api/`**: Flask-based REST API designed for production deployment (Ubuntu). Receives JSON payloads from the firmware and manages database persistence.
- **`docs/`**: Technical research, sensor suitability analysis, and installation guides.

## Getting Started
1. **Firmware**: Refer to `firmware/README.md` to build and deploy the sensor firmware.
2. **API**: Refer to `api/README.md` to deploy the backend service and database.