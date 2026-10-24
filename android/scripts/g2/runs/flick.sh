#!/usr/bin/env bash
# flick.sh <tag> [secs=8] [out dir]: records the G2's screen (screenrecord, whatever is running),
# pulls the video, extracts its real frames (no duplicates) in greyscale and runs aba.py on them:
# an A-B-A flicker score per cell. Nothing on the device is started or stopped, so it can run
# while the owner plays. Files (not for git): <out>/<tag>.mp4, <out>/<tag>-frames/, <out>/<tag>.txt.
# Then crop the reported cells from <tag>-frames/f-NNNNNN.png and look. ffmpeg comes from the
# WSL venv (~/venv-ff, as in montage.sh); PIL from the WSL system python3.
set -euo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
tag=$1; secs=${2:-8}; out=${3:-${TMPDIR:-/tmp}/flick}
mkdir -p "$out"
"$ADB" shell screenrecord --time-limit "$secs" --bit-rate 16000000 "/sdcard/flick-$tag.mp4"
"$ADB" pull "/sdcard/flick-$tag.mp4" "$out/$tag.mp4" > /dev/null
"$ADB" shell rm -f "/sdcard/flick-$tag.mp4"
W=$(wsl.exe -e wslpath -u "$(cygpath -m "$out")" | tr -d '\r')
RW=$(wsl.exe -e wslpath -u "$(cygpath -m "$R")" | tr -d '\r')
wsl.exe -e bash -c "
  cd '$W' || exit 1
  FF=\$(ls ~/venv-ff/lib/python3*/site-packages/imageio_ffmpeg/binaries/ffmpeg* | head -1)
  rm -rf '$tag-frames' && mkdir '$tag-frames'
  \$FF -hide_banner -loglevel fatal -vsync 0 -i '$tag.mp4' -vf format=gray '$tag-frames/f-%06d.png'
  python3 -I '$RW/aba.py' '$tag-frames' 60 34 12 | tee '$tag.txt'"
