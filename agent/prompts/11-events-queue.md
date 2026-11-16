# PROMPT 11 — Event queue and dual ACK

Do only this run.

LittleFS (or existing SPIFFS policy) queue: UUID + sequence + HAPRS-like columns (`docs/reference/haprs-log.md`) + MESH 485 event JSON.

Survive reboot. Retry until mesh ACK. Surface **mesh_ok** vs **device_ok** in DIAGNOSTICS and events.

Subscribe on companion pipe. Photos never stored on device.

Access card events may enqueue with `source=RFID` without finishing ACL (still stub until 17).

Next: `prompts/12-app-shell.md`.
