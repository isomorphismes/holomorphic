#!/usr/bin/env bash
set -euo pipefail

apk="${1:-artifacts/video-demo-app-debug.apk}"
activity="org.isomorphisms.analyticcontinuation.lasso.dev/org.isomorphisms.analyticcontinuation.ExplorerActivity"

adb shell wm size 270x480
adb shell wm density 160
adb install -r "$apk"
adb shell settings put secure immersive_mode_confirmations confirmed
adb logcat -c
adb shell am start -W -n "$activity"
sleep 4

adb logcat -d > video-demo.log
grep -Eq 'scenario ready: name=video-demo .*motion=1 controls=0 marker=5/1.8 field=1 .*field_budget=6' video-demo.log
grep -Fq 'clean presentation system UI hidden' video-demo.log
grep -Fq 'holomorphic field started with 3 workers' video-demo.log
grep -Fq 'zeros=4 poles=4' video-demo.log

adb exec-out screencap -p > video-demo-a.png
sleep 18
adb exec-out screencap -p > video-demo-b.png
adb logcat -d > video-demo.log

# Both scheduled exchanges finish by t=15 s. Timeline time follows real runtime
# time even when SwiftShader renders slowly; only integrated wander/q steps clamp dt.
grep -Fq 'scenario=video-demo' video-demo.log
grep -Fq 'exchanges=2/2' video-demo.log
remote_time="$(sed -n 's/.*remote_t=\([0-9][0-9.]*\).*/\1/p' video-demo.log | tail -1)"
test -n "$remote_time"
awk -v t="$remote_time" 'BEGIN { exit !(t > 12.0) }'
adb shell pidof -s org.isomorphisms.analyticcontinuation.lasso.dev | tr -d '\r' | grep -Eq '^[0-9]+$'
python3 tests/check_motion_screenshots.py video-demo-a.png video-demo-b.png --minimum-mean 1.0 --minimum-changed-fraction 0.05
! grep -Eiq 'shader compilation failed|program link failed|eglInitialize failed|could not choose GLES3 EGL config|could not create EGL surface/context|eglMakeCurrent failed|holomorphic field shader uniforms unavailable|FATAL EXCEPTION' video-demo.log
