# Website Documentation

## Prerequisites
- Python 3.x and `pip`
- Access to the running `api/` instance (for authentication)
- ArcGIS Online Account

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
   ```

2. Copy the credentials template and configure your environment:
   ```bash
   cp credentials.py.template credentials.py
   nano credentials.py
   ```
   *Note: Ensure `FLASK_SECRET_KEY` is set to a long, secure random string. Point `API_BASE_URL` to your running `api/` environment and fill out the `API_KEY` to successfully talk to the backend.*

## Running the Web App
For a development environment, you can run the app directly:
```bash
python3 app.py
```
The dashboard is now accessible on `http://localhost:3000`. Keep in mind you need to register an account first with one of your database invite codes before being able to view the dashboard!