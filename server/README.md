# Server

FastAPI + PostgreSQL. Optional; the node must work offline (`docs/architecture.md`). Excel export lives here, not on ESP32.

```
docker compose up --build
```

Health: http://localhost:8000/health

Mesh gateway: `app/mesh_gateway/` (prompt 11) using `vendor/meshcore.js` or meshcore-cli.

Photos: `PHOTO_DIR`, never synced over LoRa.
