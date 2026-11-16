# Heltec V4 panics in the ESP32-S3 AES DMA setup (gdma_new_channel,
# LoadProhibited) the moment an alarm is encrypted. The SX1262 SPI is
# already using the DMA. Software AES keeps the flood on the air.
# V3 does not take this path.

Import("env")

if not env["PIOENV"].startswith("heltec_v4"):
    Return()

from pathlib import Path

header = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env["PIOENV"] / "Crypto" / "AES.h"
text = header.read_text(encoding="utf-8")
old = "#if defined(ESP32)\n#define CRYPTO_AES_ESP32 1\n#else\n#define CRYPTO_AES_DEFAULT 1\n#endif"
new = (
    "#if !defined(ESP32)\n"
    "#define CRYPTO_AES_ESP32 1\n"
    "#else\n"
    "#define CRYPTO_AES_DEFAULT 1\n"
    "#endif"
)
if old not in text and new not in text:
    raise SystemExit("Crypto AES.h layout changed: " + str(header))
if old in text:
    header.write_text(text.replace(old, new, 1), encoding="utf-8")
    print("software AES for", env["PIOENV"])
