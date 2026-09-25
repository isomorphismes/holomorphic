from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("first", type=Path)
    parser.add_argument("second", type=Path)
    parser.add_argument("--minimum-mean", type=float, default=1.5)
    parser.add_argument("--minimum-changed-fraction", type=float, default=0.10)
    args = parser.parse_args()

    first = Image.open(args.first).convert("RGB")
    second = Image.open(args.second).convert("RGB")
    if first.size != second.size:
        raise SystemExit("motion screenshots have different dimensions")

    width, height = first.size
    box = (
        int(width * 0.10),
        int(height * 0.15),
        int(width * 0.90),
        int(height * 0.85),
    )
    diff = ImageChops.difference(first.crop(box), second.crop(box))
    mean = sum(ImageStat.Stat(diff).mean) / 3.0
    pixels = list(diff.getdata())
    changed = sum(1 for pixel in pixels if max(pixel) >= 8)
    changed_fraction = changed / max(len(pixels), 1)

    print(
        f"motion mean_abs_rgb={mean:.3f} "
        f"changed_fraction={changed_fraction:.3f}"
    )
    if mean < args.minimum_mean or changed_fraction < args.minimum_changed_fraction:
        raise SystemExit("runtime motion is still too visually weak")


if __name__ == "__main__":
    main()
