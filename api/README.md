# API Documentation (Ubuntu)

## Prerequisites
- Python 3.x and `pip`
- MySQL Server
- Nginx or Apache
- ArcGIS Online Account

## ArcGIS Online Setup
Before syncing data via `arcgis_sync.py`, you need to create a Hosted Feature Layer in ArcGIS Online to accept the data.

1. Go to your ArcGIS Online account and create a new **Hosted Feature Layer** (Point layer).
2. Add the following fields to your Feature Layer (names must match exactly):
   - `ToF_Height_mm` (Type: Integer)
   - `ToF_Quality_Grade` (Type: String)
   - `Sonic_Height_mm` (Type: Integer)
   - `Sonic_Quality_Grade` (Type: String)
   - `Measured_At` (Type: String)
3. Save the layer and copy the Feature Layer URL.
4. Add the URL to your `credentials.py` file as `ARCGIS_LAYER_URL`.

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
       synced       TINYINT(1)      DEFAULT 0,
       PRIMARY KEY (id)
   );

   CREATE TABLE invite_codes (
       id INT AUTO_INCREMENT PRIMARY KEY,
       code VARCHAR(50) UNIQUE NOT NULL,
       is_used TINYINT(1) DEFAULT 0
   );

   CREATE TABLE users (
       id INT AUTO_INCREMENT PRIMARY KEY,
       username VARCHAR(100) UNIQUE NOT NULL,
       password_hash VARCHAR(255) NOT NULL,
       name VARCHAR(100) NOT NULL,
       role ENUM('admin', 'user') DEFAULT 'user',
       created_at DATETIME DEFAULT CURRENT_TIMESTAMP
   );

   CREATE TABLE user_fields (
       user_id INT NOT NULL,
       field_id INT NOT NULL,
       PRIMARY KEY (user_id, field_id),
       FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
   );

   -- Insert invite codes for creating accounts
   INSERT INTO invite_codes (code) VALUES
       ('INVITE-CODE-1'),
       ('INVITE-CODE-2'),
       ('INVITE-CODE-3');
   ```

3. **Bootstrap First Admin Account:**
   Since admins manage all accounts, you need to create your first admin directly in the database. Run the following python command inside `api/` to generate a secure hash, then insert it:
   ```bash
   python3 -c "from werkzeug.security import generate_password_hash; print(generate_password_hash('wachtwoord123'))"
   ```
   Take that generated string and run this query inside MySQL:
   ```sql
   INSERT INTO users (username, password_hash, name, role)
   VALUES ('admin', 'PLACED_GENERATED_HASH_HERE', 'Hoofd Beheerder', 'admin');
   ```

## Production Deployment (Systemd)
To run the API as a self-contained production service:
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

   ExecStart=/path/to/project/api/.venv/bin/gunicorn --workers 1 --threads 4 --bind 127.0.0.1:5002 api:app
   Restart=always

   [Install]
   WantedBy=multi-user.target
   ```
3. Enable and start:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable api-groenwerf
   sudo systemctl start api-groenwerf
   ```

## Reverse Proxy Setup (Nginx / Apache)
Gunicorn binds to local port `5002`. You must configure your web server (Nginx or Apache) to act as a reverse proxy, accepting external HTTPS traffic on port `443` and forwarding it locally to `127.0.0.1:5002`.

## Automation
For a fully autonomous system, you might want to add a cronjob to run the `arcgis_sync.py` script periodically to sync data in the background.