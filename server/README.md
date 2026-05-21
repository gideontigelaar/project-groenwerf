# Running the server (Ubuntu)

## Prerequisites

Python 3 and pip. The server runs Flask via Gunicorn and connects to a local MySQL database.

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

Paste the following, adjusting the path to where you cloned the repo:
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

Check that it's running:
```bash
sudo systemctl status groenwerf
```

## Viewing logs

```bash
journalctl -u groenwerf -f
```
Exit with `Ctrl+C`.