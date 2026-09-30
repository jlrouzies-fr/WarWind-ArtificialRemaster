"""Build the GitHub release assets in a dist folder.

    python package_release.py --dll <mod build>\\dsound.dll --ddraw <cnc-ddraw build>\\ddraw.dll
                              --vids-hd "<War Wind>\\Data\\VIDS_HD" --out dist
      [--no-cutscenes]          mod files only
      [--part-limit BYTES]      largest cutscene archive (default 1.9 GB; GitHub's cap is 2 GiB per asset)

Writes the files installer/Install-WarWindRemaster.ps1 looks for:
  dsound.dll                   the mod (--dll)
  ddraw.dll                    cnc-ddraw built with third_party/cnc-ddraw/cnc-ddraw-backdrop.patch and
                               cnc-ddraw-wwfx.patch (--ddraw)
  WarWindHD.ini                default settings (src/mod)
  feature-hires_720.wwp        the 720p patch set (src/patchsets)
  fix-gdi-palette.wwp          keeps the game colours after Windows dialogs (src/patchsets)
  backdrop.png                 the AI-upscaled stone backdrop (assets/), installed as WarWindHD\\backdrop.png
  wwfx.hlsl                    the Modern Graphics shaders (src/mod/shaders), installed as WarWindHD\\shaders\\wwfx.hlsl
  Install-WarWindRemaster.ps1  the installer
  THIRD-PARTY-NOTICES.txt      the licences of cnc-ddraw and the code it includes (third_party/cnc-ddraw)
and one or more stored (uncompressed -- the videos are already H.264) zips per race:
WarWind-HD-Cutscenes-<RACE>-<i>of<n>.zip, each holding <RACE>/<NAME>.mp4 entries. Also writes
SHA256SUMS.txt over everything it wrote. Needs nothing beyond the standard library.
"""
import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RACES = ("EA", "OB", "OPEN", "SH", "TH")
CNC = REPO / "third_party" / "cnc-ddraw"
NOTICES = (
    ("cnc-ddraw (ddraw.dll), by FunkyFr3sh - https://github.com/FunkyFr3sh/cnc-ddraw\n"
     "ddraw.dll is cnc-ddraw built with this project's backdrop and Modern Graphics patches. The patches and the rebuild notes are in\n"
     "https://github.com/jlrouzies-fr/WarWind-ArtificialRemaster/tree/main/third_party/cnc-ddraw",
     CNC / "LICENSE"),
    ("Microsoft Detours (part of cnc-ddraw)", CNC / "LICENSE-detours.md"),
    ("LodePNG, by Lode Vandevenne (part of cnc-ddraw)", CNC / "LICENSE-lodepng.txt"),
)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def split(files, limit):
    parts, cur, size = [], [], 0
    for f in files:
        n = f.stat().st_size
        if n > limit:
            raise SystemExit(f"{f} alone is larger than the part limit")
        if cur and size + n > limit:
            parts.append(cur)
            cur, size = [], 0
        cur.append(f)
        size += n
    if cur:
        parts.append(cur)
    return parts


def is_backdrop_build(path):
    """cnc-ddraw (its version resource names it, in UTF-16) that reads the backdrop= key and
    exports the Modern Graphics entry points."""
    data = path.read_bytes()
    return ("cnc-ddraw".encode("utf-16-le") in data and b"backdrop\0" in data
            and b"WWFX_Setup\0" in data)


def write_notices(dest):
    rule = "=" * 78
    parts = ["Third-party software in War Wind - Artificial Remaster releases\n"]
    for title, lic in NOTICES:
        parts.append(f"{rule}\n{title}\n{rule}\n\n{lic.read_text(encoding='utf-8').strip()}\n")
    dest.write_text("\n".join(parts), encoding="utf-8", newline="\r\n")
    return dest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", required=True, type=Path)
    ap.add_argument("--ddraw", required=True, type=Path)
    ap.add_argument("--vids-hd", type=Path)
    ap.add_argument("--out", type=Path, default=REPO / "dist")
    ap.add_argument("--no-cutscenes", action="store_true")
    ap.add_argument("--part-limit", type=int, default=1_900_000_000)
    a = ap.parse_args()

    if b"WarWindHD.ini" not in a.dll.read_bytes():
        raise SystemExit(f"{a.dll} is not the mod")
    if a.ddraw.name.lower() != "ddraw.dll" or not is_backdrop_build(a.ddraw):
        raise SystemExit(f"{a.ddraw} is not a cnc-ddraw ddraw.dll with the backdrop and wwfx patches")

    a.out.mkdir(parents=True, exist_ok=True)
    made = []
    for src in (a.dll, a.ddraw, REPO / "src" / "mod" / "WarWindHD.ini",
                REPO / "src" / "patchsets" / "feature-hires_720.wwp",
                REPO / "src" / "patchsets" / "fix-gdi-palette.wwp", REPO / "assets" / "backdrop.png",
                REPO / "src" / "mod" / "shaders" / "wwfx.hlsl",
                REPO / "installer" / "Install-WarWindRemaster.ps1"):
        made.append(Path(shutil.copy2(src, a.out / src.name)))
    made.append(write_notices(a.out / "THIRD-PARTY-NOTICES.txt"))

    if not a.no_cutscenes:
        if not a.vids_hd:
            raise SystemExit("--vids-hd is required unless --no-cutscenes")
        for race in RACES:
            files = sorted((a.vids_hd / race).glob("*.mp4"))
            if not files:
                print(f"{race}: no videos, skipped")
                continue
            parts = split(files, a.part_limit)
            for i, group in enumerate(parts, 1):
                dest = a.out / f"WarWind-HD-Cutscenes-{race}-{i}of{len(parts)}.zip"
                with zipfile.ZipFile(dest, "w", zipfile.ZIP_STORED, allowZip64=True) as z:
                    for f in group:
                        z.write(f, f"{race}/{f.name}")
                made.append(dest)
                print(f"{dest.name}: {len(group)} videos, {dest.stat().st_size / 1e9:.2f} GB")

    (a.out / "SHA256SUMS.txt").write_text("".join(f"{sha256(p)}  {p.name}\n" for p in made))
    print(f"{len(made)} assets in {a.out}")


if __name__ == "__main__":
    main()
