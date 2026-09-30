"""List windows on the sandbox desktop (class, title, rect, child texts) without switching to it."""
import ctypes
import ctypes.wintypes as wt

from sandbox import DESKTOP_NAME

user32 = ctypes.WinDLL("user32", use_last_error=True)
EnumProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def text(h):
    buf = ctypes.create_unicode_buffer(512)
    user32.GetWindowTextW(h, buf, 512)
    return buf.value


def cls(h):
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(h, buf, 256)
    return buf.value


def rect(h):
    r = wt.RECT()
    user32.GetWindowRect(h, ctypes.byref(r))
    return (r.left, r.top, r.right, r.bottom)


def main():
    hdesk = user32.OpenDesktopW(DESKTOP_NAME, 0, False, 0x10000000)
    if not hdesk:
        raise SystemExit(f"cannot open desktop {DESKTOP_NAME}: {ctypes.get_last_error()}")
    if not user32.SetThreadDesktop(hdesk):
        raise SystemExit(f"SetThreadDesktop failed: {ctypes.get_last_error()}")

    def show(h, _):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(pid))
        print(f"hwnd={h:#x} pid={pid.value} class={cls(h)!r} title={text(h)!r} visible={bool(user32.IsWindowVisible(h))} rect={rect(h)}")
        children = []

        def child(c, _):
            children.append(f"    child class={cls(c)!r} text={text(c)!r}")
            return True

        user32.EnumChildWindows(h, EnumProc(child), 0)
        print("\n".join(children[:20]))
        return True

    user32.EnumDesktopWindows(hdesk, EnumProc(show), 0)
    user32.CloseDesktop(hdesk)


if __name__ == "__main__":
    main()
