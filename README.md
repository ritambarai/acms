# ACMS Metadata

A lightweight metadata editor for ACMS XML files, served locally via a Python HTTP server.

## Files

| File | Description |
|------|-------------|
| `server.py` | Local HTTP server with REST API (`/api/files`, `/api/load`, `/api/save`) |
| `index.html` | Metadata editor UI |
| `style.css` | Stylesheet for the UI |
| `app.js` | Frontend logic |
| `config.xml` | Configuration metadata |
| `metadata.xml` | Core metadata definitions |
| `field_value.xml` | Field value definitions |

## Usage

```bash
python3 server.py          # runs on port 8000
python3 server.py 9000     # custom port
```

Then open `http://localhost:8000` in your browser.
