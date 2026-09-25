from __future__ import annotations

import argparse
import statistics
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat


PYRAMID_SHORT_SIDES = (28, 22, 18)
DEFAULT_MIN_RAW_RGB = 1.0
DEFAULT_MIN_COARSE_GRAY = 1.0
DEFAULT_MIN_STRUCTURE_RETENTION = 0.30


def exclusion_from_file(path: Path | None) -> tuple[int, int, int] | None:
    if path is None:
        return None
    parts = path.read_text().split()
    if len(parts) != 3:
        raise SystemExit("exclusion file must contain: X Y RADIUS")
    return tuple(int(part) for part in parts)


def analysis_box(width: int, height: int) -> tuple[int, int, int, int]:
    return (
        int(width * 0.10),
        int(height * 0.15),
        int(width * 0.90),
        int(height * 0.85),
    )


def full_resolution_mask(
    size: tuple[int, int],
    box: tuple[int, int, int, int],
    exclusion: tuple[int, int, int] | None,
) -> Image.Image:
    mask = Image.new("L", (box[2] - box[0], box[3] - box[1]), 255)
    if exclusion is None:
        return mask

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
    return mask


def raw_rgb_motion(
    first: Image.Image,
    second: Image.Image,
    box: tuple[int, int, int, int],
    exclusion: tuple[int, int, int] | None,
) -> float:
    diff = ImageChops.difference(first.crop(box), second.crop(box))
    mask = full_resolution_mask(first.size, box, exclusion)
    return sum(ImageStat.Stat(diff, mask=mask).mean) / 3.0


def coarse_gray_image(
    image: Image.Image,
    box: tuple[int, int, int, int],
    target_short_side: int,
) -> Image.Image:
    crop = image.crop(box).convert("L")
    short_side = min(crop.size)
    source_pixels_per_coarse_pixel = short_side / target_short_side

    # Blur before decimation so fine phase/color bands do not alias into the
    # coarse image. This is the scale-space part of the test: only structure
    # surviving a substantial low-pass operation is allowed to count here.
    crop = crop.filter(
        ImageFilter.GaussianBlur(radius=source_pixels_per_coarse_pixel)
    )

    if crop.width <= crop.height:
        width = target_short_side
        height = max(1, round(crop.height * target_short_side / crop.width))
    else:
        height = target_short_side
        width = max(1, round(crop.width * target_short_side / crop.height))

    return crop.resize((width, height), Image.Resampling.LANCZOS)


def coarse_mask(
    coarse_size: tuple[int, int],
    box: tuple[int, int, int, int],
    exclusion: tuple[int, int, int] | None,
) -> Image.Image:
    mask = Image.new("L", coarse_size, 255)
    if exclusion is None:
        return mask

    center_x, center_y, radius = exclusion
    scale_x = coarse_size[0] / (box[2] - box[0])
    scale_y = coarse_size[1] / (box[3] - box[1])
    center_x = (center_x - box[0]) * scale_x
    center_y = (center_y - box[1]) * scale_y
    radius_x = radius * scale_x
    radius_y = radius * scale_y

    draw = ImageDraw.Draw(mask)
    draw.ellipse(
        (
            center_x - radius_x,
            center_y - radius_y,
            center_x + radius_x,
            center_y + radius_y,
        ),
        fill=0,
    )
    return mask


def coarse_gray_motion(
    first: Image.Image,
    second: Image.Image,
    box: tuple[int, int, int, int],
    exclusion: tuple[int, int, int] | None,
    target_short_side: int,
) -> float:
    first_coarse = coarse_gray_image(first, box, target_short_side)
    second_coarse = coarse_gray_image(second, box, target_short_side)
    diff = ImageChops.difference(first_coarse, second_coarse)
    mask = coarse_mask(diff.size, box, exclusion)
    return ImageStat.Stat(diff, mask=mask).mean[0]


def measure_structure_motion(
    first_path: Path,
    second_path: Path,
    exclusion: tuple[int, int, int] | None = None,
) -> tuple[float, list[float], list[float]]:
    first = Image.open(first_path).convert("RGB")
    second = Image.open(second_path).convert("RGB")
    if first.size != second.size:
        raise SystemExit("motion screenshots have different dimensions")

    box = analysis_box(*first.size)
    raw = raw_rgb_motion(first, second, box, exclusion)
    coarse = [
        coarse_gray_motion(first, second, box, exclusion, short_side)
        for short_side in PYRAMID_SHORT_SIDES
    ]
    retention = [value / raw if raw > 1.0e-9 else 0.0 for value in coarse]
    return raw, coarse, retention


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("first", type=Path)
    parser.add_argument("second", type=Path)
    parser.add_argument("--exclude-file", type=Path)
    parser.add_argument("--min-raw-rgb", type=float, default=DEFAULT_MIN_RAW_RGB)
    parser.add_argument(
        "--min-coarse-gray",
        type=float,
        default=DEFAULT_MIN_COARSE_GRAY,
    )
    parser.add_argument(
        "--min-structure-retention",
        type=float,
        default=DEFAULT_MIN_STRUCTURE_RETENTION,
    )
    args = parser.parse_args()

    exclusion = exclusion_from_file(args.exclude_file)
    raw, coarse, retention = measure_structure_motion(
        args.first,
        args.second,
        exclusion,
    )
    median_coarse = statistics.median(coarse)
    median_retention = statistics.median(retention)

    coarse_text = ",".join(f"{value:.3f}" for value in coarse)
    retention_text = ",".join(f"{value:.3f}" for value in retention)
    print(
        f"Cauchy structure raw_rgb={raw:.3f} "
        f"coarse_gray=[{coarse_text}] "
        f"retention=[{retention_text}] "
        f"median_retention={median_retention:.3f}"
    )

    if raw < args.min_raw_rgb:
        raise SystemExit("running APK is too static for a structure-motion test")
    if median_coarse < args.min_coarse_gray:
        raise SystemExit("large-scale grayscale structure is too static")
    if median_retention < args.min_structure_retention:
        raise SystemExit(
            "motion is dominated by fine color/texture churn rather than "
            "large-scale structure"
        )


if __name__ == "__main__":
    main()
