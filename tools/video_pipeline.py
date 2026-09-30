"""Remaster War Wind cutscenes: 320x240@15 Cinepak -> 1920x1080@30 H.264.

Per video:
  1. ffmpeg extracts PNG frames and the audio track.
  2. Real-ESRGAN upscales frames 4x (1280x960).
  3. RIFE doubles the frame rate (15 -> 30 fps).
  4. ffmpeg scales the picture to 1386x1080, centres it on the stone
     background (the main-menu art) and encodes H.264 with x264. The whole set
     (both variants) stays under 1 GB; 4K NVENC output was ~9 GB for no
     visible gain over a 320x240 source.
     Videos with captions also get a "_sub" variant with burned-in subtitles.

Outputs mirror the game layout: <out>/<RACE>/<NAME>.mp4 (and <NAME>_sub.mp4).
Stages are cached per video, so an interrupted batch resumes where it stopped.
Progress shows on stderr as a batch bar (videos done, ETA) and a stage bar for
the current video; every finished video is appended to <work>/render.log.

Usage:
    python video_pipeline.py background            # build the 4K pillarbox background
    python video_pipeline.py run [NAME ...]        # process all videos or the named ones
      [--frames N]   only process the first N source frames (quick test)
      [--out DIR]    write <DIR>/<RACE>/<NAME>.mp4 instead of <game>/Data/VIDS_HD
"""
import argparse
import shutil
import subprocess
import threading
import time
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageFilter
from tqdm import tqdm

import cap2ass
import d3gr
from res import DATA, GAME, res

TOOLS = Path(__file__).resolve().parent
BIN = TOOLS / "bin"
FFMPEG = next(BIN.glob("ffmpeg-*/bin/ffmpeg.exe"))
FFPROBE = FFMPEG.with_name("ffprobe.exe")
ESRGAN = BIN / "esrgan" / "realesrgan-ncnn-vulkan.exe"
RIFE_DIR = next(BIN.glob("rife-ncnn-vulkan-*"))
RIFE = RIFE_DIR / "rife-ncnn-vulkan.exe"

WORK = TOOLS.parent / "video_work"
OUT = GAME / "Data" / "VIDS_HD"
BACKGROUND = WORK / "background_4k.png"
SUBS = WORK / "subs"
LOG = WORK / "render.log"

MENU_BACKGROUND = (4, 5)     # RES.004 entry 5: carved-stone main menu art
MENU_PALETTE = (1, 1)        # RES.001 entry 1
BG_W, BG_H = 3840, 2160      # stone backdrop, also cnc-ddraw's backdrop.png
OUT_W, OUT_H = 1920, 1080    # cutscene frame
EDGE_CROP = 6                # source pixels trimmed per side: the AVIs carry a bright stripe at x=2..5
SRC_W, SRC_H, SCALE = 320, 240, 4
SOURCE_FPS = 15                  # Cinepak AVIs repeat frames as empty chunks; cfr output re-emits them


def pic_width(height):
    """Width of the cropped 4:3 picture fitted to `height` (2772 at 2160, 1386 at 1080)."""
    return (height * (SRC_W - 2 * EDGE_CROP) // SRC_H) & ~1



def run(cmd):
    subprocess.run([str(c) for c in cmd], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


class Progress:
    """Two stderr bars (batch with ETA, current video's stage) plus the render log."""

    def __init__(self, count):
        self.batch = tqdm(total=count, unit="video", desc="War Wind HD", position=0, dynamic_ncols=True)
        self.current = tqdm(position=1, leave=False, dynamic_ncols=True)
        self.name = ""
        self.started = 0.0

    def begin(self, name):
        self.name = name
        self.started = time.monotonic()

    @contextmanager
    def stage(self, label, folder=None, total=None):
        """Show `label` while the body runs; with `folder`, count its PNGs every second."""
        bar = self.current
        bar.bar_format = None if folder else "{desc} [{elapsed}]"
        bar.reset(total=total)
        bar.set_description_str(f"{self.name} {label}")
        stop = threading.Event()
        poller = threading.Thread(target=self._count, args=(folder, stop), daemon=True)
        if folder:
            poller.start()
        try:
            yield
        finally:
            stop.set()
            if folder:
                poller.join()
                bar.n = total
                bar.refresh()

    def _count(self, folder, stop):
        while not stop.wait(1):
            self.current.n = sum(1 for _ in folder.glob("*.png"))
            self.current.refresh()

    def finish(self, source, dest):
        seconds = time.monotonic() - self.started
        line = f"{datetime.now():%Y-%m-%d %H:%M:%S}  {self.name:<8} {seconds:5.0f}s  {source} -> {dest}"
        with LOG.open("a", encoding="utf-8") as log:
            log.write(line + "\n")
        tqdm.write(line)
        self.batch.update(1)

    def close(self):
        self.current.close()
        self.batch.close()


def build_background():
    """Upscale the menu art 4x and crop it to a dimmed 16:9 backdrop."""
    WORK.mkdir(parents=True, exist_ok=True)
    frame = d3gr.frames(res(MENU_BACKGROUND[0])[MENU_BACKGROUND[1]])[0]
    src = WORK / "menu_640.png"
    d3gr.to_image(frame, d3gr.palette(res(MENU_PALETTE[0])[MENU_PALETTE[1]])).convert("RGB").save(src)
    up = WORK / "menu_2560.png"
    run([ESRGAN, "-i", src, "-o", up, "-n", "realesrgan-x4plus", "-s", "4"])
    img = Image.open(up).convert("RGB").resize((BG_W, BG_W * 3 // 4), Image.LANCZOS)
    top = (img.height - BG_H) // 2
    img = img.crop((0, top, BG_W, top + BG_H))
    img = Image.blend(Image.new("RGB", img.size), img, 0.6)
    shade = Image.new("L", img.size, 0)
    pic_w = pic_width(BG_H)
    margin = (BG_W - pic_w) // 2
    shade.paste(255, (margin - 60, 0, margin + pic_w + 60, BG_H))
    shade = shade.filter(ImageFilter.GaussianBlur(50))
    img = Image.composite(Image.new("RGB", img.size), img, shade.point(lambda v: v * 0.8))
    img.save(BACKGROUND)
    return BACKGROUND


def source_videos(names):
    vids = sorted((DATA / "VIDS").glob("*/*.AVI"))
    if names:
        wanted = {n.upper() for n in names}
        vids = [v for v in vids if v.stem.upper() in wanted]
    return vids


def extract(avi, work, max_frames, progress):
    frames = work / "src"
    if not (work / "extract.done").exists():
        shutil.rmtree(frames, ignore_errors=True)
        frames.mkdir(parents=True)
        limit = ["-frames:v", str(max_frames)] if max_frames else []
        with progress.stage("extract"):
            run([FFMPEG, "-y", "-i", avi, *limit, "-vf", f"fps={SOURCE_FPS}", frames / "%06d.png"])
            run([FFMPEG, "-y", "-i", avi, "-vn", "-ac", "2", "-ar", "48000", work / "audio.wav"])
        (work / "extract.done").touch()
    return frames


def upscale(src, work, progress):
    out = work / "up"
    if not (work / "upscale.done").exists():
        shutil.rmtree(out, ignore_errors=True)
        out.mkdir()
        with progress.stage("upscale", out, len(list(src.glob("*.png")))):
            run([ESRGAN, "-i", src, "-o", out, "-n", "realesrgan-x4plus", "-s", "4", "-f", "png"])
        (work / "upscale.done").touch()
    return out


def interpolate(src, work, progress):
    out = work / "interp"
    if not (work / "interp.done").exists():
        shutil.rmtree(out, ignore_errors=True)
        out.mkdir()
        count = len(list(src.glob("*.png")))
        with progress.stage("interpolate", out, count * 2):
            run([RIFE, "-i", src, "-o", out, "-m", RIFE_DIR / "rife-v4.6", "-n", str(count * 2), "-f", "%08d.png"])
        (work / "interp.done").touch()
    return out


def video_background():
    """The backdrop scaled to the cutscene frame, cached next to the 4K one."""
    out = WORK / f"background_{OUT_H}p.png"
    if not out.exists() or out.stat().st_mtime < BACKGROUND.stat().st_mtime:
        Image.open(BACKGROUND).resize((OUT_W, OUT_H), Image.LANCZOS).save(out)
    return out


def encode(frames, work, dest, subtitle, duration, progress):
    dest.parent.mkdir(parents=True, exist_ok=True)
    pic_w = pic_width(OUT_H)
    margin = (OUT_W - pic_w) // 2
    crop = EDGE_CROP * SCALE
    # The AVIs end on held (empty) frames, so the extracted frames can be shorter than the
    # soundtrack: hold the last picture, pad the audio, and cut at the source duration.
    graph = (f"[1:v]crop=in_w-{2 * crop}:in_h:{crop}:0,scale={pic_w}:{OUT_H}:flags=lanczos,"
             f"tpad=stop_mode=clone:stop_duration=60[pic];"
             f"[0:v][pic]overlay={margin}:0:shortest=1")
    if subtitle:
        sub = subtitle.as_posix().replace(":", "\\:")
        graph += f",subtitles='{sub}'"
    graph += ",format=yuv420p[v]"
    with progress.stage("encode subtitled" if subtitle else "encode"):
        run([FFMPEG, "-y", "-loop", "1", "-framerate", "30", "-i", video_background(),
             "-framerate", "30", "-i", frames / "%08d.png", "-i", work / "audio.wav",
             "-filter_complex", graph, "-map", "[v]", "-map", "2:a", "-af", "apad", "-t", f"{duration:.3f}",
             "-c:v", "libx264", "-preset", "veryslow", "-crf", "24", "-maxrate", "8M", "-bufsize", "16M",
             "-profile:v", "high", "-level", "4.1", "-c:a", "aac", "-b:a", "64k", "-ac", "1",
             "-movflags", "+faststart", dest])


def source_duration(avi):
    """Length of the source AVI in seconds (container duration: covers video and audio)."""
    out = subprocess.run([str(FFPROBE), "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(avi)],
                         check=True, capture_output=True, text=True).stdout
    return float(out.strip())


def process(avi, captions, max_frames, out, progress):
    name = avi.stem.upper()
    work = WORK / (f"test_{name}" if max_frames else name)
    work.mkdir(parents=True, exist_ok=True)
    progress.begin(name)
    frames = interpolate(upscale(extract(avi, work, max_frames, progress), work, progress), work, progress)
    dest = out / avi.parent.name.upper() / f"{name}.mp4"
    if max_frames:
        dest = WORK / "test_out" / f"{name}.mp4"
    duration = source_duration(avi)
    encode(frames, work, dest, None, duration, progress)
    if name in captions:
        encode(frames, work, dest.with_name(f"{name}_sub.mp4"), SUBS / f"{name}.ass", duration, progress)
    progress.finish(avi.relative_to(DATA), dest)
    return dest


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("background")
    r = sub.add_parser("run")
    r.add_argument("names", nargs="*")
    r.add_argument("--frames", type=int, default=0)
    r.add_argument("--out", type=Path, default=OUT)
    a = ap.parse_args()
    if a.cmd == "background":
        print(build_background())
        return
    if not BACKGROUND.exists():
        build_background()
    captions = cap2ass.write_all(SUBS)
    vids = source_videos(a.names)
    progress = Progress(len(vids))
    for avi in vids:
        process(avi, captions, a.frames, a.out, progress)
    progress.close()


if __name__ == "__main__":
    main()
