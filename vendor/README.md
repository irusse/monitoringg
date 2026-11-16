# Upstream sources

Cloned into this directory. Treat them as **read-only dependencies**.

Application code lives in `/firmware`, `/server`, `/app`, `/protocol`.
Copy or adapt **only the pieces listed in `docs/vendor-map.md`**.

## Do not

- Fork MeshCore unless a MeshCore bug blocks Heltec V4.
- Patch Companion Protocol.
- Send employee photos over LoRa.
- Replace WiegandNG with a homemade decoder.

## Refresh clones

```powershell
.\tools\clone-vendor.ps1
```

## Current clones

| Directory | Upstream |
|---|---|
| MeshCore | https://github.com/meshcore-dev/MeshCore |
| esp32-rfid | https://github.com/pvtex/esp32-rfid |
| esp-rfid | https://github.com/esprfid/esp-rfid |
| eeh-esp32-rfid | https://github.com/smford/eeh-esp32-rfid |
| HAPRS | https://github.com/rahul-chatterjee-27/HAPRS |
| meshcore-open | https://github.com/zjs81/meshcore-open |
| meshcore-client | https://github.com/OffbandMesh/meshcore-client |
| meshcore.js | https://github.com/meshcore-dev/meshcore.js |
| meshcore-cli | https://github.com/fdlamotte/meshcore-cli |
| pidoors | https://github.com/sybethiesant/pidoors |
| esp32-rfid-access-control | https://github.com/darthshadoww/esp32-rfid-access-control |
| WiegandNG | https://github.com/pvtex/Wiegand-NG-Multi-Bit-Wiegand-Library-for-Arduino |
| hass-hubc2000pp | https://github.com/Regressor/hass-hubc2000pp |
