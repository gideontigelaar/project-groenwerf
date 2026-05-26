# Project Groenwerf
A grass height measurement system for ride-on mowers using a Raspberry Pi Pico 2W to send sensor data to a remote server.

- `firmware/` - Firmware code for the Pico, handles sensors and data processing
- `server/` - Flask server, receives data and stores it in a MySQL database
- `docs/` - Project research and documentation

The Pico reads sensor data and posts batches to the server over WiFi. The server stores readings in a database.