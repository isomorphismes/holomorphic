#!/usr/bin/env bash
set -euo pipefail

apk="${1:-artifacts/app-debug.apk}"
activity="org.isomorphisms.analyticcontinuation.lasso.dev/org.isomorphisms.analyticcontinuation.ExplorerActivity"

adb install -r "$apk"
adb shell settings put secure immersive_mode_confirmations confirmed
adb logcat -c
adb shell am start -W -n "$activity"
sleep 4

adb logcat -d > holomorphic-emulator.log
grep -Fq 'scenario ready: name=interactive' holomorphic-emulator.log
grep -Fq 'holomorphic field started with 3 workers' holomorphic-emulator.log
grep -Fq 'holomorphic field ready:' holomorphic-emulator.log
grep -Fq 'zeros=1 poles=1' holomorphic-emulator.log
grep -Fq 'holomorphic field first frame:' holomorphic-emulator.log

steps_before="$(sed -n 's/.*holomorphic field: workers=3 steps=\([0-9][0-9]*\).*/\1/p' holomorphic-emulator.log | tail -1)"
test -n "$steps_before"
test "$steps_before" -gt 0

adb exec-out screencap -p > holomorphic-motion-a.png
sleep 6
adb exec-out screencap -p > holomorphic-motion-b.png
adb logcat -d > holomorphic-emulator.log

steps_after_motion="$(sed -n 's/.*holomorphic field: workers=3 steps=\([0-9][0-9]*\).*/\1/p' holomorphic-emulator.log | tail -1)"
test -n "$steps_after_motion"
test "$steps_after_motion" -gt "$steps_before"
python3 tests/check_motion_screenshots.py holomorphic-motion-a.png holomorphic-motion-b.png

read -r width height < <(
    sed -n 's/.*holomorphic field ready: surface=\([0-9][0-9]*\)x\([0-9][0-9]*\).*/\1 \2/p' holomorphic-emulator.log | tail -1
)
test -n "$width"
test -n "$height"

min_side="$width"
if (( height < width )); then
    min_side="$height"
fi
placement_radius=$((48 * min_side / 1000))
if (( placement_radius < 26 )); then placement_radius=26; fi
if (( placement_radius > 38 )); then placement_radius=38; fi
zero_control_x=$((placement_radius + 16))
control_y=$((height - placement_radius - 16))
pole_control_x=$((zero_control_x + 2 * placement_radius + 14))

adb shell input tap "$zero_control_x" "$control_y"
adb shell input tap "$((width / 2 - 100))" "$((height / 2 + 80))"
sleep 1
adb shell input tap "$pole_control_x" "$control_y"
adb shell input tap "$((width / 2 + 120))" "$((height / 2 - 70))"
sleep 2

adb logcat -d > holomorphic-emulator.log
grep -Fq 'placement selected: pole' holomorphic-emulator.log
grep -Eq 'zero added: .*count=2' holomorphic-emulator.log
grep -Eq 'pole added: .*count=2' holomorphic-emulator.log
steps_after_edit="$(sed -n 's/.*holomorphic field: workers=3 steps=\([0-9][0-9]*\).*/\1/p' holomorphic-emulator.log | tail -1)"
test -n "$steps_after_edit"
test "$steps_after_edit" -gt "$steps_after_motion"
grep -Eq 'holomorphic field: workers=3 steps=[0-9]+ .*zeros=2 poles=2' holomorphic-emulator.log
! grep -Eiq 'shader compilation failed|program link failed|eglInitialize failed|could not choose GLES3 EGL config|could not create EGL surface/context|eglMakeCurrent failed|holomorphic field shader uniforms unavailable|FATAL EXCEPTION' holomorphic-emulator.log
adb exec-out screencap -p > holomorphic-emulator.png
