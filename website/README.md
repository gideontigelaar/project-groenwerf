# Website Documentation (Ubuntu)

## Prerequisites
- Python 3.x and `pip`
- Access to the running `api/` instance (for authentication)
- ArcGIS Online Account
- Nginx or Apache

## ArcGIS Fields Setup (Polygon Layer)
The dashboard groups points into designated fields based on polygon boundaries. You need to create a Hosted Feature Layer containing these Polygons.

1. Go to your ArcGIS Online account and create a new **Hosted Feature Layer** (Polygon layer).
2. Add the following fields to your Feature Layer:
   - `Name` (Type: String) - *The visual name of the field (e.g. "Veld 1")*
3. Save the layer and draw/import your field boundary polygons onto it.
4. Copy the Feature Layer URL and configure it in `credentials.py` as `ARCGIS_FIELDS_URL`.

## Setup
1. In the `website/` directory, create and activate a virtual environment:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   playwright install chromium
   ```

2. Copy the credentials template and configure your environment:
   ```bash
   cp credentials.py.template credentials.py
   nano credentials.py
   ```
   *Note: Ensure `FLASK_SECRET_KEY` is set to a long, secure random string. Point `API_BASE_URL` to your running `api/` environment and fill out the `API_KEY` to successfully talk to the backend.*

## Development
For a development environment, you can run the app directly:
```bash
python3 app.py
```
The dashboard is now accessible on `http://localhost:3000`. Keep in mind you need to register an account first with one of your database invite codes before being able to view the dashboard!

## Production Deployment (Systemd)
To run the Website as a self-contained production service:
1. Create the service file: `sudo nano /etc/systemd/system/web-groenwerf.service`
2. Configure the service:
   ```ini
   [Unit]
   Description=Groenwerf Flask Website
   After=network.target

   [Service]
   User=your_username
   Group=www-data
   WorkingDirectory=/path/to/project/website
   Environment="PATH=/path/to/project/website/.venv/bin"

   ExecStart=/path/to/project/website/.venv/bin/gunicorn --workers 1 --threads 4 --bind 127.0.0.1:5003 app:app
   Restart=always

   [Install]
   WantedBy=multi-user.target
   ```
3. Enable and start:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable web-groenwerf
   sudo systemctl start web-groenwerf
   ```

## Reverse Proxy Setup (Nginx / Apache)
Gunicorn binds to local port `3000`. You must configure your web server (Nginx or Apache) to act as a reverse proxy, accepting external HTTPS/HTTP traffic on port `443`/`80` and forwarding it locally to `127.0.0.1:3000`.