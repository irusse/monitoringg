# PROMPT 14 — Allowed CONTROL + capabilities

Do only this run.

`COMMAND_EXECUTE` only if `Device.capabilities` contains the op. Roles VIEWER / OPERATOR / ADMIN.

UI: extra confirmation for CONTROL. Show mesh ACK vs device ACK vs timeout.

**Forbidden:** a generic disable-fire-alarm / disarm-all command. Relays only if the adapter maps a named capability and the panel config allows it.

Update `docs/bolid.md` / `docs/security.md` with the allow-list you actually implemented.

Next: `prompts/15-signal10.md`.
