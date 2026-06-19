# Project Groenwerf

A grass height measurement system for ride-on mowers, utilizing a Raspberry Pi Pico 2W to process sensor data and transmit it to a remote server. The system features a secure dashboard with useful insights.

## Project Structure
- **`firmware/`**: Embedded C++ source code for the Raspberry Pi Pico 2W (Mac/Dev). Handles sensor polling, data processing, and network transmission.
- **`api/`**: Flask-based REST API designed for production deployment (Ubuntu). Receives JSON payloads from the firmware, handles ArcGIS synchronization, and manages the MySQL database.
- **`website/`**: Flask-based web portal rendering the frontend dashboard and fetching insights dynamically from the `api/` and ArcGIS layers. Requires user authentication.
- **`docs/`**: Technical research, sensor suitability analysis, and installation guides.

## Getting Started
1. **API Setup (Backend)**: Refer to `api/README.md` to deploy the backend service, setup the database containing the `invite_codes` table, and configure API endpoints.
2. **Website Setup (Frontend)**: Refer to `website/README.md` to set up your ArcGIS Polygon layers and launch the web dashboard.
3. **Firmware**: Refer to `firmware/README.md` to build and deploy the sensor firmware to the Pico 2W.