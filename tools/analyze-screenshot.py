#!/usr/bin/env python3
"""Analyses a screenshot of the desktop that LivePaper captured.

It tells whether the video is visible on the screen, and whether the desktop
icons are there: the icons are in the top left corner of the screen, and they
are the only bright, small objects on a dark background.

Usage: analyze-screenshot.py <file.bmp> [more files...]
"""

import sys
from PIL import Image


def analyze(path):
    image = Image.open(path).convert("RGB")
    width, height = image.size
    pixels = image.load()
    print(f"== {path} ({width}x{height}) ==")

    # The colours of the whole screen, and of the top left corner, where the
    # desktop icons are.
    def stats(x0, y0, x1, y1):
        total = 0
        sum_r = sum_g = sum_b = 0
        brightest = 0
        for y in range(y0, y1, 2):
            for x in range(x0, x1, 2):
                r, g, b = pixels[x, y]
                total += 1
                sum_r += r
                sum_g += g
                sum_b += b
                brightest = max(brightest, r + g + b)
        if total == 0:
            return None
        return (sum_r // total, sum_g // total, sum_b // total), brightest

    overall, overall_max = stats(0, 0, width, height)
    print(f"  the whole screen: average colour {overall}, brightest sum {overall_max}")

    # The corner of the desktop icons: a region of 260x200 pixels.
    corner, corner_max = stats(0, 0, min(260, width), min(200, height))
    print(f"  the corner of the icons: average colour {corner}, brightest sum {corner_max}")

    # How many different colours are in the corner? The icons are many small
    # objects on the background of the wallpaper.
    colours = set()
    for y in range(0, min(200, height), 2):
        for x in range(0, min(260, width), 2):
            colours.add(pixels[x, y])
    print(f"  the corner of the icons has {len(colours)} different colours")

    # The centre of the screen, where the video is.
    centre, centre_max = stats(width // 4, height // 4, 3 * width // 4, 3 * height // 4)
    print(f"  the centre of the screen: average colour {centre}, brightest sum {centre_max}")

    # The pixels of a horizontal line, to see what the screen looks like.
    print("  the middle row (every 120 pixels):")
    row = []
    for x in range(0, width, 120):
        row.append("%02x%02x%02x" % pixels[x, height // 2])
    print("    " + " ".join(row))
    print()


if __name__ == "__main__":
    for name in sys.argv[1:]:
        analyze(name)
