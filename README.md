# Project Groenwerf

Grass height monitoring system using a Raspberry Pi Pico 2W with sensors, sending data to a remote server.

- `firmware/` — C++ code for the Pico, handles sensors and sending data
- `server/` — Python Flask server, receives data and stores it in MySQL
- `docs/` — additional documentation

The Pico reads sensor data and POSTs batches to the server over WiFi. The server stores readings in a MySQL database and serves a basic dashboard.

## Credentials

Both parts of the project use a credentials file that is never committed.

- Firmware: copy `firmware/include/credentials.h.template` to `firmware/include/credentials.h`
- Server: copy `server/credentials.py.template` to `server/credentials.py`

Fill in your values in each file before building or running.