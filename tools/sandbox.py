"""Run WW.EXE on a separate, invisible Win32 desktop with a display-mode watchdog.

The game never appears on the user's desktop, cannot take focus or the cursor,
is muted in the Windows volume mixer (every audio session it opens), and is killed
immediately if any monitor's display mode changes while it runs.

Usage:
    python sandbox.py run [--seconds N] [--ini ddraw.dev.d3d9.ini]   # blocks; stop early by creating STOP file
    python sandbox.py stop                # asks a running sandbox to shut down
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time
from pathlib import Path

GAME_DIR = Path(__file__).resolve().parents[4]
EXE = GAME_DIR / "WW.EXE"
STOP_FILE = Path(__file__).with_name("sandbox.STOP")
DEV_INI = Path(__file__).with_name("ddraw.dev.ini")
GAME_INI = GAME_DIR / "ddraw.ini"
LOG_FILE = Path(__file__).with_name("sandbox.log")
DESKTOP_NAME = "WarWindHD_Sandbox"

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

GENERIC_ALL = 0x10000000
ENUM_CURRENT_SETTINGS = 0xFFFFFFFF
CREATE_NEW_PROCESS_GROUP = 0x00000200
STARTF_USESHOWWINDOW = 0x1
SW_SHOWNOACTIVATE = 4


class DEVMODEW(ctypes.Structure):
    _fields_ = [
        ("dmDeviceName", wt.WCHAR * 32), ("dmSpecVersion", wt.WORD), ("dmDriverVersion", wt.WORD),
        ("dmSize", wt.WORD), ("dmDriverExtra", wt.WORD), ("dmFields", wt.DWORD),
        ("dmPositionX", wt.LONG), ("dmPositionY", wt.LONG), ("dmDisplayOrientation", wt.DWORD),
        ("dmDisplayFixedOutput", wt.DWORD), ("dmColor", ctypes.c_short), ("dmDuplex", ctypes.c_short),
        ("dmYResolution", ctypes.c_short), ("dmTTOption", ctypes.c_short), ("dmCollate", ctypes.c_short),
        ("dmFormName", wt.WCHAR * 32), ("dmLogPixels", wt.WORD), ("dmBitsPerPel", wt.DWORD),
        ("dmPelsWidth", wt.DWORD), ("dmPelsHeight", wt.DWORD), ("dmDisplayFlags", wt.DWORD),
        ("dmDisplayFrequency", wt.DWORD), ("dmICMMethod", wt.DWORD), ("dmICMIntent", wt.DWORD),
        ("dmMediaType", wt.DWORD), ("dmDitherType", wt.DWORD), ("dmReserved1", wt.DWORD),
        ("dmReserved2", wt.DWORD), ("dmPanningWidth", wt.DWORD), ("dmPanningHeight", wt.DWORD),
    ]


class DISPLAY_DEVICEW(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("DeviceName", wt.WCHAR * 32), ("DeviceString", wt.WCHAR * 128),
                ("StateFlags", wt.DWORD), ("DeviceID", wt.WCHAR * 128), ("DeviceKey", wt.WCHAR * 128)]


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("lpReserved", wt.LPWSTR), ("lpDesktop", wt.LPWSTR),
                ("lpTitle", wt.LPWSTR), ("dwX", wt.DWORD), ("dwY", wt.DWORD), ("dwXSize", wt.DWORD),
                ("dwYSize", wt.DWORD), ("dwXCountChars", wt.DWORD), ("dwYCountChars", wt.DWORD),
                ("dwFillAttribute", wt.DWORD), ("dwFlags", wt.DWORD), ("wShowWindow", wt.WORD),
                ("cbReserved2", wt.WORD), ("lpReserved2", ctypes.c_void_p), ("hStdInput", wt.HANDLE),
                ("hStdOutput", wt.HANDLE), ("hStdError", wt.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", wt.HANDLE), ("hThread", wt.HANDLE),
                ("dwProcessId", wt.DWORD), ("dwThreadId", wt.DWORD)]


def log(msg):
    line = time.strftime("%H:%M:%S ") + msg
    print(line, flush=True)
    with LOG_FILE.open("a", encoding="utf-8") as f:
        f.write(line + "\n")


def display_modes():
    """Return {device: (w, h, bpp, hz)} for every attached monitor."""
    modes = {}
    i = 0
    while True:
        dd = DISPLAY_DEVICEW()
        dd.cb = ctypes.sizeof(dd)
        if not user32.EnumDisplayDevicesW(None, i, ctypes.byref(dd), 0):
            break
        i += 1
        if not dd.StateFlags & 0x1:  # DISPLAY_DEVICE_ATTACHED_TO_DESKTOP
            continue
        dm = DEVMODEW()
        dm.dmSize = ctypes.sizeof(dm)
        if user32.EnumDisplaySettingsW(dd.DeviceName, ENUM_CURRENT_SETTINGS, ctypes.byref(dm)):
            modes[dd.DeviceName] = (dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmDisplayFrequency)
    return modes


def mute_process(pid):
    """Mute every audio session owned by pid; sessions appear as the game opens streams."""
    from pycaw.pycaw import AudioUtilities

    muted = 0
    for session in AudioUtilities.GetAllSessions():
        if session.ProcessId == pid and not session.SimpleAudioVolume.GetMute():
            session.SimpleAudioVolume.SetMute(1, None)
            muted += 1
    return muted


def run(seconds, dev_ini_path=DEV_INI):
    if not EXE.exists():
        sys.exit(f"missing {EXE}")
    dev_ini = Path(dev_ini_path).read_text(errors="replace")
    if "windowed=true" not in dev_ini.lower() or "fullscreen=true" in dev_ini.lower():
        sys.exit("refusing to start: ddraw.dev.ini must be windowed=true and fullscreen=false")
    user_ini = GAME_INI.read_text(errors="replace")
    GAME_INI.write_text(dev_ini)
    STOP_FILE.unlink(missing_ok=True)
    baseline = display_modes()
    log(f"baseline display modes: {baseline}")

    user32.CreateDesktopW.restype = wt.HANDLE
    hdesk = user32.CreateDesktopW(DESKTOP_NAME, None, None, 0, GENERIC_ALL, None)
    if not hdesk:
        sys.exit(f"CreateDesktop failed: {ctypes.get_last_error()}")

    si = STARTUPINFOW()
    si.cb = ctypes.sizeof(si)
    si.lpDesktop = DESKTOP_NAME
    si.dwFlags = STARTF_USESHOWWINDOW
    si.wShowWindow = SW_SHOWNOACTIVATE
    pi = PROCESS_INFORMATION()
    ok = kernel32.CreateProcessW(str(EXE), None, None, None, False, CREATE_NEW_PROCESS_GROUP,
                                 None, str(GAME_DIR), ctypes.byref(si), ctypes.byref(pi))
    if not ok:
        user32.CloseDesktop(hdesk)
        sys.exit(f"CreateProcess failed: {ctypes.get_last_error()}")
    log(f"WW.EXE pid {pi.dwProcessId} started on hidden desktop '{DESKTOP_NAME}'")

    reason = "timeout"
    deadline = time.time() + seconds
    try:
        while time.time() < deadline:
            if kernel32.WaitForSingleObject(pi.hProcess, 100) == 0:
                reason = "game exited"
                break
            if mute_process(pi.dwProcessId):
                log("muted new audio session(s)")
            now = display_modes()
            if now != baseline:
                kernel32.TerminateProcess(pi.hProcess, 1)
                user32.ChangeDisplaySettingsExW(None, None, None, 0, None)
                reason = f"DISPLAY MODE CHANGED {baseline} -> {now}; game killed, mode restored"
                break
            if STOP_FILE.exists():
                reason = "stop requested"
                break
    finally:
        kernel32.TerminateProcess(pi.hProcess, 0)
        kernel32.WaitForSingleObject(pi.hProcess, 5000)
        kernel32.CloseHandle(pi.hProcess)
        kernel32.CloseHandle(pi.hThread)
        user32.CloseDesktop(hdesk)
        STOP_FILE.unlink(missing_ok=True)
        GAME_INI.write_text(user_ini)
        log(f"sandbox ended: {reason}; display modes now {display_modes()}")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--seconds", type=float, default=600)
    r.add_argument("--ini", default=str(DEV_INI), help="ddraw profile to run with (must be windowed)")
    sub.add_parser("stop")
    sub.add_parser("modes")
    a = ap.parse_args()
    if a.cmd == "run":
        run(a.seconds, a.ini)
    elif a.cmd == "stop":
        STOP_FILE.touch()
    else:
        print(display_modes())


if __name__ == "__main__":
    main()
