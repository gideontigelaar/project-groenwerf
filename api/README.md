# API Documentation (Ubuntu)

## Prerequisites
- Python 3.x and `pip`
- MySQL Server

## Setup
1. In the `api/` directory, create and activate a virtual environment:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   ```

2. Copy the credentials template and configure your environment:
   ```bash
   cp credentials.py.template credentials.py
   nano credentials.py
   ```
   *Note: Ensure `DB_HOST` is correctly set to your database server's IP.*

## Database Setup
1. Log into your MySQL server as `root` and execute the following:
   ```sql
   CREATE DATABASE IF NOT EXISTS groenwerf;
   CREATE USER 'groenwerf_admin'@'%' IDENTIFIED BY 'YOUR_SECURE_PASSWORD';
   GRANT ALL PRIVILEGES ON groenwerf.* TO 'groenwerf_admin'@'%';
   FLUSH PRIVILEGES;
   ```

2. Initialize the table structure:
   ```sql
   USE groenwerf;
   CREATE TABLE sensor_readings (
       id           INT UNSIGNED    NOT NULL AUTO_INCREMENT,
       latitude     DECIMAL(10, 7)  DEFAULT NULL,
       longitude    DECIMAL(10, 7)  DEFAULT NULL,
       tof_mm       SMALLINT UNSIGNED DEFAULT NULL,
       sonic_mm     SMALLINT UNSIGNED DEFAULT NULL,
       temperature  DECIMAL(5, 2)   DEFAULT NULL,
       sonic_raw_mm SMALLINT UNSIGNED DEFAULT NULL,
       tof_raw_mm   SMALLINT UNSIGNED DEFAULT NULL,
       accel_raw_x  FLOAT           DEFAULT NULL,
       accel_raw_y  FLOAT           DEFAULT NULL,
       accel_raw_z  FLOAT           DEFAULT NULL,
       measured_at  DATETIME        DEFAULT NULL,
       PRIMARY KEY (id)
   );
   ```

## Production Deployment (Systemd)
To run the API as a service on Ubuntu:
1. Create the service file: `sudo nano /etc/systemd/system/api-groenwerf.service`
2. Configure the service:
   ```ini
   [Unit]
   Description=Groenwerf Flask API
   After=network.target

   [Service]
   User=your_username
   Group=www-data
   WorkingDirectory=/path/to/project/api
   Environment="PATH=/path/to/project/api/.venv/bin"
   ExecStart=/path/to/project/api/.venv/bin/gunicorn --bind 127.0.0.1:5002 api:app
   Restart=always

   [Install]
   WantedBy=multi-user.target
   ```
3. Enable and start: `sudo systemctl daemon-reload && sudo systemctl enable api-groenwerf && sudo systemctl start api-groenwerf`

## API Usage & Authentication
All requests to the `/sensor-data` endpoints (both `GET` and `POST`) must include your secure API key in the headers.

**Header Format:**
`X-API-Key: YOUR_SECRET_API_KEY`

**Example GET Request:**
```bash
curl -H "X-API-Key: YOUR_SECRET_API_KEY" "[http://127.0.0.1:5002/sensor-data?limit=10](http://127.0.0.1:5002/sensor-data?limit=10)"
```