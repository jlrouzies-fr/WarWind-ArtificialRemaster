"""Convert War Wind's Data/VIDS.CAP caption table into ASS subtitle files.

VIDS.CAP sections look like:
    [WWOPEN]
    CAPTION = 20 450 0  540 750 1 ...
Each triple is (start frame, end frame, caption index) at the AVI's 15 fps.
Caption index N is text entry CAPTION_TEXT_BASE + N in RES.000.
"""
import re
from pathlib import Path

from res import DATA, strings

CAPTION_TEXT_BASE = 746
SOURCE_FPS = 15.0

# Layout for a 3840x2160 frame whose 4:3 picture spans x = 480..3360.
ASS_HEADER = """[Script Info]
ScriptType: v4.00+
PlayResX: 3840
PlayResY: 2160
WrapStyle: 0
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Caption,Palatino Linotype,84,&H00E8F4FF,&H000000FF,&H00101010,&H80000000,0,0,0,0,100,100,0,0,1,5,3,2,600,600,110,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""


def parse_cap(path=DATA / "VIDS.CAP"):
    """Return {video name: [(start_frame, end_frame, caption_index), ...]}."""
    captions, current = {}, None
    for line in Path(path).read_text(encoding="latin1").splitlines():
        line = line.strip()
        if m := re.fullmatch(r"\[(\w+)\]", line):
            current = m.group(1).upper()
        elif current and line.upper().startswith("CAPTION"):
            nums = [int(x) for x in line.split("=", 1)[1].split()]
            captions[current] = [tuple(nums[i:i + 3]) for i in range(0, len(nums) - 2, 3)]
    return captions


def _timestamp(frame):
    t = frame / SOURCE_FPS
    h, rem = divmod(t, 3600)
    m, s = divmod(rem, 60)
    return f"{int(h)}:{int(m):02d}:{s:05.2f}"


def _escape(text):
    text = text.replace("\x01", " ").replace("{", "(").replace("}", ")")
    return re.sub(r"\s{2,}", "  ", text).strip()


def to_ass(entries, texts):
    lines = [ASS_HEADER]
    for start, end, index in entries:
        text = _escape(texts[CAPTION_TEXT_BASE + index])
        lines.append(f"Dialogue: 0,{_timestamp(start)},{_timestamp(end)},Caption,,0,0,0,,{text}\n")
    return "".join(lines)


def write_all(out_dir):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    texts = strings()
    caps = parse_cap()
    for name, entries in caps.items():
        (out_dir / f"{name}.ass").write_text(to_ass(entries, texts), encoding="utf-8")
    return caps


if __name__ == "__main__":
    import sys

    caps = write_all(sys.argv[1] if len(sys.argv) > 1 else "subs")
    print(f"{len(caps)} subtitle files")
