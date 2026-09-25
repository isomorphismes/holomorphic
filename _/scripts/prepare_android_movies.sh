#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 RENDERED_DIRECTORY OUTPUT_DIRECTORY" >&2
    exit 2
fi

rendered_directory=$1
output_directory=$2
mkdir -p "$output_directory"

for movie_name in zeta-open-close rational-disc-reveal; do
    source_movie=$(find "$rendered_directory" -type f -name "${movie_name}.mp4" -print -quit)
    if [[ -z "$source_movie" ]]; then
        echo "missing rendered movie: ${movie_name}.mp4" >&2
        exit 1
    fi

    output_movie="${output_directory}/${movie_name}.mp4"
    ffmpeg -v error -y \
        -i "$source_movie" \
        -an \
        -vf scale=640:360:flags=lanczos \
        -c:v libx264 \
        -preset medium \
        -crf 18 \
        -profile:v baseline \
        -level:v 3.0 \
        -pix_fmt yuv420p \
        -x264-params bframes=0:cabac=0:ref=1 \
        -movflags +faststart \
        "$output_movie"

    profile=$(ffprobe -v error -select_streams v:0 -show_entries stream=profile \
        -of default=noprint_wrappers=1:nokey=1 "$output_movie")
    level=$(ffprobe -v error -select_streams v:0 -show_entries stream=level \
        -of default=noprint_wrappers=1:nokey=1 "$output_movie")
    pixel_format=$(ffprobe -v error -select_streams v:0 -show_entries stream=pix_fmt \
        -of default=noprint_wrappers=1:nokey=1 "$output_movie")
    b_frames=$(ffprobe -v error -select_streams v:0 -show_entries stream=has_b_frames \
        -of default=noprint_wrappers=1:nokey=1 "$output_movie")

    [[ "$profile" == "Constrained Baseline" || "$profile" == "Baseline" ]]
    [[ "$level" == "30" ]]
    [[ "$pixel_format" == "yuv420p" ]]
    [[ "$b_frames" == "0" ]]

    duration=$(ffprobe -v error -show_entries format=duration \
        -of default=noprint_wrappers=1:nokey=1 "$output_movie")
    sample_time=$(awk -v duration="$duration" 'BEGIN { print duration / 2.0 }')
    statistics_file=$(mktemp)
    ffmpeg -v error -ss "$sample_time" -i "$output_movie" \
        -vf "signalstats,metadata=print:file=${statistics_file}" \
        -frames:v 1 -f null -
    awk -F= '
        /lavfi.signalstats.YAVG/ { average = $2 }
        /lavfi.signalstats.YMAX/ { maximum = $2 }
        END { exit !(average > 25.0 && maximum > 80.0) }
    ' "$statistics_file"
    rm -f "$statistics_file"
done
