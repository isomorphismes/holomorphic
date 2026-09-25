#!/usr/bin/env bash
set -euo pipefail
set -x

artifact_dir=build/properly-typed
package=org.isomorphisms.analyticcontinuation.lasso.dev
component=org.isomorphisms.analyticcontinuation.lasso.dev/org.isomorphisms.analyticcontinuation.ExplorerActivity

assert_renderer_log() {
    local log=$1
    grep -Fq 'holomorphic field renderer ready:' "$log"
    if grep -Eiq 'shader compilation failed|program link failed|holomorphic field shader uniforms unavailable|FATAL EXCEPTION' "$log"; then
        cat "$log"
        return 1
    fi
}

adb install -r "$artifact_dir/handwritten-core-frozen.apk"
adb logcat -c
adb shell am start -W -n "$component"
sleep 4
adb logcat -d > "$artifact_dir/handwritten-core-emulator.log"
assert_renderer_log "$artifact_dir/handwritten-core-emulator.log"
adb exec-out screencap -p > "$artifact_dir/handwritten-core.png"

adb shell am force-stop "$package"
adb install -r "$artifact_dir/properly-typed-core-frozen.apk"
adb logcat -c
adb shell am start -W -n "$component"
sleep 4
adb logcat -d > "$artifact_dir/properly-typed-core-emulator.log"
assert_renderer_log "$artifact_dir/properly-typed-core-emulator.log"
adb exec-out screencap -p > "$artifact_dir/properly-typed-core.png"

python3 - <<'PY'
from PIL import Image, ImageChops, ImageStat

handwritten = Image.open('build/properly-typed/handwritten-core.png').convert('RGB')
typed = Image.open('build/properly-typed/properly-typed-core.png').convert('RGB')
if handwritten.size != typed.size:
    raise SystemExit('comparison screenshots have different dimensions')
width, height = handwritten.size
box = (
    int(width * 0.10),
    int(height * 0.15),
    int(width * 0.90),
    int(height * 0.85),
)
first = handwritten.crop(box)
second = typed.crop(box)
diff = ImageChops.difference(first, second)
mean = sum(ImageStat.Stat(diff).mean) / 3.0
pixels = list(diff.getdata())
changed = sum(1 for pixel in pixels if max(pixel) >= 8)
changed_fraction = changed / max(len(pixels), 1)
maximum = max((max(pixel) for pixel in pixels), default=0)
print(
    f'handwritten-vs-typed mean_abs_rgb={mean:.4f} '
    f'changed_fraction_ge8={changed_fraction:.6f} max_channel={maximum}'
)
if mean > 1.0 or changed_fraction > 0.02:
    raise SystemExit('compiler-generated core diverges materially from handwritten core')
PY

adb shell am force-stop "$package"
adb install -r "$artifact_dir/properly-typed-probe.apk"
adb logcat -c
adb shell am start -W -n "$component"
sleep 4
adb logcat -d > "$artifact_dir/properly-typed-live-emulator.log"
assert_renderer_log "$artifact_dir/properly-typed-live-emulator.log"
grep -Fq 'holomorphic field first frame:' "$artifact_dir/properly-typed-live-emulator.log"

steps_before=$(sed -n 's/.*holomorphic field: workers=3 steps=\([0-9][0-9]*\).*/\1/p' \
    "$artifact_dir/properly-typed-live-emulator.log" | tail -1)
test -n "$steps_before"
adb exec-out screencap -p > "$artifact_dir/properly-typed-motion-a.png"
sleep 6
adb exec-out screencap -p > "$artifact_dir/properly-typed-motion-b.png"
adb logcat -d > "$artifact_dir/properly-typed-live-emulator.log"
steps_after=$(sed -n 's/.*holomorphic field: workers=3 steps=\([0-9][0-9]*\).*/\1/p' \
    "$artifact_dir/properly-typed-live-emulator.log" | tail -1)
test -n "$steps_after"
test "$steps_after" -gt "$steps_before"

python3 - <<'PY'
from PIL import Image, ImageChops, ImageStat

first = Image.open('build/properly-typed/properly-typed-motion-a.png').convert('RGB')
second = Image.open('build/properly-typed/properly-typed-motion-b.png').convert('RGB')
if first.size != second.size:
    raise SystemExit('motion screenshots have different dimensions')
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
print(f'typed live motion mean_abs_rgb={mean:.3f} changed_fraction={changed_fraction:.3f}')
if mean < 1.5 or changed_fraction < 0.10:
    raise SystemExit('compiler-generated core is not visibly evolving')
PY
