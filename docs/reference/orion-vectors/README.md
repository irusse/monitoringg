# Orion CRC fixtures

Hex dumps of **bus frames** only. No decrypted vendor firmware binaries.

Source of the three frames: [Habr 563584](https://habr.com/ru/articles/563584/) (worked examples that cite [CXEM «Протокол Орион (Болид)»](https://forum.cxem.net/topic/105126-%D0%BF%D1%80%D0%BE%D1%82%D0%BE%D0%BA%D0%BE%D0%BB-%D0%BE%D1%80%D0%B8%D0%BE%D0%BD-%D0%B1%D0%BE%D0%BB%D0%B8%D0%B4/), topic started 2012-03-21). Captured 2026-08-28. CXEM post HTML is JS-gated; these published strings are the legal vectors.

Native copy: `firmware/heltec_v4/test/native/orion_vectors.h`.

| id | Frame (hex) | CRC (last) | `crc8_orion` on prefix |
|---|---|---|---|
| habr_set_global_key | `03 06 00 11 BA BA 8D` | `8D` | pass |
| habr_status_req_encrypted | `83 08 00 ED B8 BA BA BA 62` | `62` | pass |
| habr_status_rsp_encrypted | `83 0A E2 B8 BA BE B9 7D 2F 72 D7` | `D7` | pass |

Dallas/Maxim (same algo) **matches**. Generic CRC-8 poly `0x07` **mismatches**. Flipped bit → mismatch. See `docs/reference/orion-frames.md`.
