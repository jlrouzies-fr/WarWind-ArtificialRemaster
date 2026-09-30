"""Install or remove the War Wind HD mod in the game folder.

    python deploy.py install [--dev]   # copy dsound.dll (the mod), WarWindHD.ini, patch files, shaders,
                                       # the menu backdrop and our cnc-ddraw build (ddraw.dll)
    python deploy.py uninstall         # remove the mod files (VIDS_HD and ddraw.dll stay)
    python deploy.py cmd "capture a.bmp" "key 1B" ...   # queue developer commands

--dev enables the developer command channel used with sandbox.py.
"""
import shutil
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PROJECT = TOOLS.parent
GAME = PROJECT.parents[2]
MOD_BUILD = PROJECT / "mod" / "build" / "dsound.dll"
MOD_INI = PROJECT / "mod" / "WarWindHD.ini"
MOD_SHADERS = PROJECT / "mod" / "shaders"
PATCHSETS = PROJECT / "patchsets"
INSTALLED_PATCHES = GAME / "WarWindHD"
CNC_DDRAW_BUILD = GAME / "cnc-ddraw" / "bin" / "Release" / "ddraw.dll"
# 4K stone art shown by cnc-ddraw around 640x480 screens (ddraw.ini backdrop=WarWindHD\backdrop.png)
BACKDROP = PROJECT / "video_work" / "background_4k.png"


def install(dev):
    shutil.copy2(MOD_BUILD, GAME / "dsound.dll")
    ini = MOD_INI.read_text()
    if dev:
        ini = ini.replace("Commands=0", "Commands=1")
    (GAME / "WarWindHD.ini").write_text(ini)
    INSTALLED_PATCHES.mkdir(exist_ok=True)
    for old in INSTALLED_PATCHES.glob("*.wwp"):
        old.unlink()
    for wwp in PATCHSETS.glob("*.wwp"):
        shutil.copy2(wwp, INSTALLED_PATCHES / wwp.name)
    shutil.copy2(BACKDROP, INSTALLED_PATCHES / "backdrop.png")
    shaders = INSTALLED_PATCHES / "shaders"
    shaders.mkdir(exist_ok=True)
    for hlsl in MOD_SHADERS.glob("*.hlsl"):
        shutil.copy2(hlsl, shaders / hlsl.name)
    shutil.copy2(CNC_DDRAW_BUILD, GAME / "ddraw.dll")
    print(f"installed into {GAME} (dev={dev})")


def uninstall():
    for name in ("dsound.dll", "WarWindHD.ini", "WarWindHD.log", "WarWindHD.cmd"):
        (GAME / name).unlink(missing_ok=True)
    shutil.rmtree(INSTALLED_PATCHES, ignore_errors=True)
    print("uninstalled")


def queue_commands(lines):
    cmd = GAME / "WarWindHD.cmd"
    while cmd.exists():
        time.sleep(0.05)
    tmp = cmd.with_suffix(".tmp")
    tmp.write_text("\n".join(lines) + "\n")
    tmp.replace(cmd)


if __name__ == "__main__":
    verb = sys.argv[1]
    if verb == "install":
        install("--dev" in sys.argv)
    elif verb == "uninstall":
        uninstall()
    elif verb == "cmd":
        queue_commands(sys.argv[2:])
    else:
        sys.exit(__doc__)
