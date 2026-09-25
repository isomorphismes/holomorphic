#!/usr/bin/env bash
set -euo pipefail

log_file=${1:-holomorphic-emulator.log}
coordinate_file=${2:-/tmp/cauchy-repeated-zero-cluster}

read -r width height < <(
    sed -n 's/.*Cauchy field ready: surface=\([0-9][0-9]*\)x\([0-9][0-9]*\).*/\1 \2/p' \
        "$log_file" | tail -1
)

test -n "${width:-}"
test -n "${height:-}"

min_side=$width
if (( height < width )); then
    min_side=$height
fi

pixel_radius=$((42 * min_side / 100))
cluster_x=$((width / 2 - 34 * pixel_radius / 100))
cluster_y=$((height / 2))
exclude_radius=$((22 * min_side / 100))

# The app starts with one zero at z=-0.34. Leave it in place and add seven
# more within a few pixels. This models an order-eight repeated root without
# depending on a drag gesture merely to construct the runtime fixture.
for offset in \
    '2 0' \
    '-2 1' \
    '1 -2' \
    '-1 -2' \
    '3 2' \
    '-3 2' \
    '0 3'
do
    read -r dx dy <<< "$offset"
    adb shell input tap "$((cluster_x + dx))" "$((cluster_y + dy))"
done

printf '%s %s %s\n' "$cluster_x" "$cluster_y" "$exclude_radius" > "$coordinate_file"
