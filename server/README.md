# Running the server (Ubuntu)

## Prerequisites
Python 3 and pip. The server runs Flask via Gunicorn and connects to a MySQL database.

## First-time setup
In `server/`:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Copy the credentials template and fill in your values:
```bash
cp credentials.py.template credentials.py
nano credentials.py
```

The API key must match the one in `firmware/include/credentials.h`. Generate a new one with:
```bash
openssl rand -hex 32
```

## Database setup
Create the database and tables:
```sql
CREATE DATABASE groenwerf;
USE groenwerf;

CREATE TABLE sensor_readings (
    id              INT UNSIGNED    NOT NULL AUTO_INCREMENT,
    latitude        DECIMAL(10, 7)  DEFAULT NULL,
    longitude       DECIMAL(10, 7)  DEFAULT NULL,
    tof_mm          SMALLINT UNSIGNED DEFAULT NULL,
    sonic_final_mm  SMALLINT UNSIGNED DEFAULT NULL,
    temperature     DECIMAL(5, 2)   DEFAULT NULL,
    measured_at     DATETIME        DEFAULT NULL,
    PRIMARY KEY (id)
);

CREATE TABLE raw_sensor_readings (
    id           INT UNSIGNED    NOT NULL AUTO_INCREMENT,
    sonic_raw_mm SMALLINT UNSIGNED DEFAULT NULL,
    tof_raw_mm   SMALLINT UNSIGNED DEFAULT NULL,
    accel_raw_x  FLOAT           DEFAULT NULL,
    accel_raw_y  FLOAT           DEFAULT NULL,
    accel_raw_z  FLOAT           DEFAULT NULL,
    measured_at  DATETIME        DEFAULT NULL,
    PRIMARY KEY (id)
);

CREATE TABLE reading_pairs (
    raw_id       INT UNSIGNED NOT NULL,
    processed_id INT UNSIGNED NOT NULL,
    PRIMARY KEY (raw_id, processed_id),
    FOREIGN KEY (raw_id)       REFERENCES raw_sensor_readings(id),
    FOREIGN KEY (processed_id) REFERENCES sensor_readings(id)
);
```

## Running manually
In `server/`:
```bash
source .venv/bin/activate
gunicorn --bind 0.0.0.0:5002 app:app
```
Runs as long as the terminal stays open. Use the systemd setup below for production.

## Running as a service
Create the service file:
```bash
sudo nano /etc/systemd/system/groenwerf.service
```

Paste the following, adjusting path to where you cloned the repo:
```ini
[Unit]
Description=Groenwerf Flask Server
After=network.target

[Service]
User=root
WorkingDirectory=/root/project-groenwerf/server
Environment="PATH=/root/project-groenwerf/server/.venv/bin"
ExecStart=/root/project-groenwerf/server/.venv/bin/gunicorn --bind 0.0.0.0:5002 app:app
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start it:
```bash
sudo systemctl daemon-reload
sudo systemctl enable groenwerf
sudo systemctl start groenwerf
```

Check if it's running:
```bash
sudo systemctl status groenwerf
```

## Viewing logs
```bash
journalctl -u groenwerf -f
```
Exit with `Ctrl+C`.