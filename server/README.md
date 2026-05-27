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

*Note: If this Flask server is hosted on a different machine than your MySQL database, ensure `DB_HOST` in `credentials.py` is set to the database server's public IP address, not `localhost`.*

The API key must match the one in `firmware/include/credentials.h`. Generate a new one with:
```bash
openssl rand -hex 32
```

## Database setup
First, log into your MySQL server as `root` and create the database and a dedicated user with remote access permissions:
```sql
CREATE DATABASE groenwerf;
CREATE USER 'groenwerf_admin'@'%' IDENTIFIED BY 'YOUR_SECURE_PASSWORD';
GRANT ALL PRIVILEGES ON groenwerf.* TO 'groenwerf_admin'@'%';
FLUSH PRIVILEGES;
```

Then, switch to the database and create the unified sensor readings table:
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