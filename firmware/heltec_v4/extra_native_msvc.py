# Put a g++.bat shim on PATH so PlatformIO's native env can compile with MSVC.
Import("env")  # pylint: disable=undefined-variable

import os
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT = Path(env.subst("$PROJECT_DIR"))
SHIM_SRC = PROJECT / "tools" / "native_gxx_shim.py"
SHIM_DIR = Path(env.subst("$BUILD_DIR")) / "gxx_shim"


def _vcvars_bat():
    p = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")
    if p.is_file():
        return str(p)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / (
        "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if not vswhere.is_file():
        return None
    out = subprocess.check_output(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"],
        text=True,
    ).strip()
    bat = Path(out) / "VC/Auxiliary/Build/vcvars64.bat"
    return str(bat) if bat.is_file() else None


def _apply_vcvars(bat):
    dumped = subprocess.check_output(
        'cmd /c ""{}" >nul && set"'.format(bat),
        shell=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    for line in dumped.splitlines():
        if "=" not in line:
            continue
        key, _, val = line.partition("=")
        os.environ[key] = val
        env["ENV"][key] = val  # noqa: F821


def _short_path(p):
    """8.3 path so g++.bat stays ASCII when the profile name is Unicode."""
    try:
        import ctypes
        GetShortPathNameW = ctypes.windll.kernel32.GetShortPathNameW
        GetShortPathNameW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint]
        buf = ctypes.create_unicode_buffer(512)
        if GetShortPathNameW(str(p), buf, 512):
            return buf.value
    except Exception:
        pass
    return str(p)


if os.name == "nt":
    bat = _vcvars_bat()
    if not bat:
        raise RuntimeError("native_protocol_tests: need Visual Studio cl.exe (or install MinGW g++)")
    _apply_vcvars(bat)
    SHIM_DIR.mkdir(parents=True, exist_ok=True)
    py = _short_path(sys.executable)
    shim = _short_path(SHIM_SRC)
    for name in ("g++.bat", "g++.cmd", "gcc.bat"):
        (SHIM_DIR / name).write_text(
            '@echo off\r\n"{}" "{}" %*\r\n'.format(py, shim),
            encoding="ascii",
            errors="replace",
        )
    path = str(SHIM_DIR) + os.pathsep + env["ENV"].get("PATH", os.environ.get("PATH", ""))
    env["ENV"]["PATH"] = path
    os.environ["PATH"] = path
    env.Replace(  # noqa: F821
        CXX=[sys.executable, str(SHIM_SRC)],
        CC=[sys.executable, str(SHIM_SRC)],
        LINK=[sys.executable, str(SHIM_SRC)],
    )
    if not shutil.which("cl.exe"):
        raise RuntimeError("vcvars64 ran but cl.exe is not on PATH")
