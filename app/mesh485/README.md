# MESH 485 console (`app/mesh485`)

One Flutter app for **Android and Windows** (`docs/apps.md`). Same screens: Dashboard, Objects, Devices, Events, Mesh, Bridge, Access, Settings.

This is **not** MeshCore Open. No chat, contacts, channels, or QR.

## Transport seam

`Mesh485Client` + `FixtureMesh485Client` serve in-memory MESH 485 JSON (Device / Point / Event ids `RAW-001` / `TS-001` / `PP-001`). Live USB CDC (Windows) and BLE (Android) are **prompt 13**. Companion binary frames are not parsed here.

## Run

SDK is not assumed on PATH. Example:

```
C:\flutter\bin\flutter.bat run -d windows
C:\flutter\bin\flutter.bat run -d chrome
C:\flutter\bin\flutter.bat test
```

Android: `flutter devices` then `flutter run -d <id>` if an SDK/device is present.

Login is a stub `viewer` chip. Mesh is extender health (RSSI/SNR/RX/TX/uptime), not radio chat. Bridge is a READ_ONLY placeholder. Access is a TS-001 card (no OPEN / employees).
