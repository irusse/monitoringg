#!/usr/bin/env python3
"""Translate g++-style -o/-c into MSVC cl.exe. Used when MinGW is not installed."""
from __future__ import print_function

import os
import shutil
import subprocess
import sys


def cl_exe():
    found = shutil.which("cl.exe")
    if found:
        return found
    print("native shim: cl.exe not on PATH (run vcvars64)", file=sys.stderr)
    return None


def main(argv):
    cl = cl_exe()
    if not cl:
        return 1
    compile_only = "-c" in argv
    out = None
    srcs = []
    flags = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-o" and i + 1 < len(argv):
            out = argv[i + 1]
            i += 2
            continue
        if a == "-c":
            i += 1
            continue
        if a.startswith("-std=") or a in ("-Os", "-O2", "-O3", "-g", "-Wall", "-Wextra"):
            i += 1
            continue
        if a.endswith(".cpp") or a.endswith(".c") or a.endswith(".cc") or a.endswith(".o") or a.endswith(".obj"):
            srcs.append(a)
            i += 1
            continue
        flags.append(a)
        i += 1

    if compile_only:
        if not out or not srcs:
            print("native shim: missing -o or source", file=sys.stderr)
            return 2
        cmd = [cl, "/nologo", "/EHsc", "/c", "/Fo" + out] + flags + srcs
    else:
        if not out:
            print("native shim: missing -o for link", file=sys.stderr)
            return 2
        cmd = [cl, "/nologo", "/F8388608", "/Fe" + out] + srcs + flags

    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
