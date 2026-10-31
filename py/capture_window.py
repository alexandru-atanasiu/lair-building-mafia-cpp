"""Screenshots and pokes a running game window from outside, for ship builds, which have no dev
driver. Windows messages are posted, so it works without focus (but the real mouse still counts
when it moves over the window). Prefer the dev driver (py/dev_client.py) in dbg and opt.

    python py/capture_window.py "wait 2" "shot shots/a.png" "key tab" "click 400 300" "wait 1" "shot shots/b.png" close

Commands: shot <png>, key <tab|space|1|2|3>, move <x> <y>, click <x> <y>, wait <seconds>, close.
Coordinates are client pixels. --title picks the window by the start of its title.
"""

import argparse
import ctypes
import os
import sys
import time
from ctypes import wintypes

from PIL import Image

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
user32.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = wintypes.HBITMAP
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wintypes.HDC]
gdi32.GetDIBits.argtypes = [wintypes.HDC, wintypes.HBITMAP, wintypes.UINT, wintypes.UINT, ctypes.c_void_p,
                            ctypes.c_void_p, wintypes.UINT]

WM_CLOSE, WM_KEYDOWN, WM_KEYUP = 0x0010, 0x0100, 0x0101
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0200, 0x0201, 0x0202
PW_CLIENTONLY, PW_RENDERFULLCONTENT = 1, 2
KEYS = {"tab": 0x09, "space": 0x20, "1": 0x31, "2": 0x32, "3": 0x33}


def find_window(title, timeout=60.0):
    found = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def callback(hwnd, _):
        length = user32.GetWindowTextLengthW(hwnd)
        buffer = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buffer, length + 1)
        if buffer.value.startswith(title) and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    deadline = time.time() + timeout
    while True:
        user32.EnumWindows(callback_type(callback), 0)
        if found:
            return found[0]
        if time.time() > deadline:
            sys.exit(f"No window titled '{title}...'")
        time.sleep(0.2)


def shot(hwnd, path):
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    width, height = rect.right, rect.bottom
    screen_dc = user32.GetDC(hwnd)
    memory_dc = gdi32.CreateCompatibleDC(screen_dc)
    bitmap = gdi32.CreateCompatibleBitmap(screen_dc, width, height)
    previous = gdi32.SelectObject(memory_dc, bitmap)
    if not user32.PrintWindow(hwnd, memory_dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT):
        sys.exit("PrintWindow failed")

    class BitmapInfoHeader(ctypes.Structure):
        _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG), ("biHeight", wintypes.LONG),
                    ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                    ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                    ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD), ("biClrImportant", wintypes.DWORD)]

    header = BitmapInfoHeader(ctypes.sizeof(BitmapInfoHeader), width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(memory_dc, bitmap, 0, height, pixels, ctypes.byref(header), 0)
    gdi32.SelectObject(memory_dc, previous)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(hwnd, screen_dc)

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    Image.frombuffer("RGBA", (width, height), pixels, "raw", "BGRA", 0, 1).convert("RGB").save(path)
    print(f"{path}: {width}x{height}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--title", default="Capo di Tutti Capi")
    parser.add_argument("commands", nargs="+")
    args = parser.parse_args()
    hwnd = find_window(args.title)

    for command in args.commands:
        word, *rest = command.split()
        if word == "shot":
            shot(hwnd, " ".join(rest))
        elif word == "wait":
            time.sleep(float(rest[0]))
        elif word == "key":
            user32.PostMessageW(hwnd, WM_KEYDOWN, KEYS[rest[0]], 0)
            time.sleep(0.1)
            user32.PostMessageW(hwnd, WM_KEYUP, KEYS[rest[0]], 0)
        elif word in ("move", "click"):
            position = int(rest[0]) | (int(rest[1]) << 16)
            user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, position)
            time.sleep(0.1)
            if word == "click":
                user32.PostMessageW(hwnd, WM_LBUTTONDOWN, 1, position)
                time.sleep(0.1)
                user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, position)
        elif word == "close":
            user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        else:
            sys.exit(f"Unknown command '{command}'")
        time.sleep(0.05)
    return 0


if __name__ == "__main__":
    sys.exit(main())
