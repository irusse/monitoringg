# 3MAP command list

See `docs/protocol-3map.md`.

JSON debug example:

```json
{"v":1,"type":"access.command","id":"8A12F2","device":"TS-001","cmd":"STATUS","operator":"admin","ts":1787912345,"nonce":"c1f0","payload":{}}
```

Binary packing: CBOR or packed struct, must fit MeshCore payload (MESH 485 cascade). Access commands are a profile inside MESH 485 — `docs/mesh485-protocol.md`.
