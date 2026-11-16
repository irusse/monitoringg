# Master prompt

**MESH 485 Extender:** Heltec V4 = MeshCore transport + isolated RS-485 HAL + device adapters.

Not a Bolid-only firmware. Not a MeshCore messenger.

```
MeshCore → MESH 485 Protocol → Device Abstraction → Adapter → RS-485
```

First adapters: Bolid (С2000 / Сигнал-10 / Сигнал-20), Modbus RTU, RAW. Access/turnstile is `PROFILE_ACCESS` on the same image (Wiegand already in phases 01–04).

Modes: MONITOR (default) · BRIDGE (authorized) · CONTROLLER (capability-gated).

Fire/security: this node is an **integration/monitoring gateway**, never the sole path to silence or disarm. Default privilege is READ_ONLY.

Heltec OLED: product UI (`docs/oled-ui.md`), not MeshCore chat. GPIO 19/20 = USB.

**Next session:** paste `agent/prompts/08-device-adapters.md` only. Phases 01–07 are done.
