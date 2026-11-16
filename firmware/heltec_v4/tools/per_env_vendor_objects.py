# src_dir is "." and build_src_filter pulls ../../vendor/MeshCore.
# With the default BUILD_SRC_DIR ($BUILD_DIR/src) those objects land in
# .pio/build/vendor, shared by every env. V3 (hardware AES) and V4
# (software AES, see force_v4_software_aes.py) then link each other's
# MeshCore objects and the V4 link fails on AES128_ESP.
#
# Two extra levels keep ../../vendor inside $BUILD_DIR:
#   $BUILD_DIR/fw/src/src/...      <- firmware sources
#   $BUILD_DIR/vendor/MeshCore/... <- vendor objects, per env

Import("env")

import os

env.Replace(BUILD_SRC_DIR=os.path.join("$BUILD_DIR", "fw", "src"))
