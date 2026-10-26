# PlatformIO pre-script for env:sim: finds SDL2 and adds its compile/link flags.
#   Windows:     MSYS2 (MSYS2_ROOT, default C:/msys64) with mingw-w64-x86_64-SDL2
#   Linux/macOS: sdl2-config, or pkg-config sdl2 as a fallback
import os
import shutil
import subprocess
import sys

Import("env")


def query(cmd):
    try:
        return subprocess.check_output(cmd, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


if sys.platform == "win32":
    root = os.environ.get("MSYS2_ROOT", "C:/msys64").replace("\\", "/")
    env.Append(
        CPPPATH=[root + "/mingw64/include/SDL2"],
        LIBPATH=[root + "/mingw64/lib"],
        LIBS=["SDL2"],
    )
else:
    flags = None
    if shutil.which("sdl2-config"):
        flags = query(["sdl2-config", "--cflags", "--libs"])
    if not flags and shutil.which("pkg-config"):
        flags = query(["pkg-config", "--cflags", "--libs", "sdl2"])
    if not flags:
        sys.stderr.write(
            "SDL2 not found: install libsdl2-dev (Debian/Ubuntu) or sdl2 (Homebrew)\n"
        )
        env.Exit(1)
    env.MergeFlags(flags)
