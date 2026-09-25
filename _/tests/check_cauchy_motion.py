from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat


DEFAULT_MIN_MEAN_ABS_RGB = 1.5
DEFAULT_MIN_CHANGED_FRACTION = 0.10
PIXEL_CHANGE_THRESHOLD = 8


def exclusion_from_file(path: Path | None) -> tuple[int, int, int] | None:
    if path is None:
        return None
    parts = path.read_text().split()
    if len(parts) != 3:
        raise SystemExit("exclusion file must contain: X Y RADIUS")
    return tuple(int(part) for part in parts)


def measure_motion(
    first_path: Path,
    second_path: Path,
    exclusion: tuple[int, int, int] | None = None,
) -> tuple[float, float]:
    first = Image.open(first_path).convert("RGB")
    second = Image.open(second_path).convert("RGB")
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
    mask = Image.new("L", diff.size, 255)

    if exclusion is not None:
        center_x, center_y, radius = exclusion
        center_x -= box[0]
        center_y -= box[1]
        draw = ImageDraw.Draw(mask)
        draw.ellipse(
            (
                center_x - radius,
                center_y - radius,
                center_x + radius,
                center_y + radius,
            ),
            fill=0,
        )

    mean_abs_rgb = sum(ImageStat.Stat(diff, mask=mask).mean) / 3.0
    changed = 0
    eligible = 0
    for pixel, selected in zip(diff.getdata(), mask.getdata()):
        if not selected:
            continue
        eligible += 1
        if max(pixel) >= PIXEL_CHANGE_THRESHOLD:
            changed += 1
    changed_fraction = changed / max(eligible, 1)
    return mean_abs_rgb, changed_fraction


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("first", type=Path)
    parser.add_argument("second", type=Path)
    parser.add_argument("--exclude-file", type=Path)
    parser.add_argument("--min-mean", type=float, default=DEFAULT_MIN_MEAN_ABS_RGB)
    parser.add_argument(
        "--min-changed-fraction",
        type=float,
        default=DEFAULT_MIN_CHANGED_FRACTION,
    )
    args = parser.parse_args()

    exclusion = exclusion_from_file(args.exclude_file)
    mean_abs_rgb, changed_fraction = measure_motion(
        args.first,
        args.second,
        exclusion,
    )
    print(
        f"Cauchy motion mean_abs_rgb={mean_abs_rgb:.3f} "
        f"changed_fraction={changed_fraction:.3f}"
    )
    if mean_abs_rgb < args.min_mean or changed_fraction < args.min_changed_fraction:
        raise SystemExit("Cauchy-field motion is still too visually weak")


if __name__ == "__main__":
    main()
