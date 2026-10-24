#!/usr/bin/env bash
# movie_pause.sh <tag> <pause secs> [menu-keys]: a pause in a prerecorded movie and the resume
# after it, on U3's intro movie (New Game; a Videodec2 movie, pausable with Options). Options at
# the 25th second of the movie, optional presses in the pause menu (comma-separated pad.sh
# buttons: "down,right,up" switches the subtitles and returns to Continue), <pause secs> of
# waiting, Continue (cross), then 70 s of watching whether frames keep coming; a hang gets a
# thread dump (pad.sh dump). Written for the castle movie hang of 2026-10-24 (not reproduced
# here). SAVES_BACKUP required (u3_ab.sh); OUT for the logs and screenshots.
set -uo pipefail
export MSYS_NO_PATHCONV=1
R=$(cd -- "$(dirname -- "$0")" && pwd)
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
F=/sdcard/Android/data/dev.unchartedg2.game/files
export OUT=${OUT:-${TMPDIR:-/tmp}/movie}
: "${SAVES_BACKUP:?SAVES_BACKUP=<saves backup directory in files/>}"
mkdir -p "$OUT"
P="bash $R/pad.sh"
tag=$1; pause=$2; keys=${3:-}
"$ADB" shell pidof dev.unchartedg2.game > /dev/null && { echo "the game is running, abort"; exit 1; }
bash "$R/u3_ab.sh" start "$tag" u3_720f.bin 2>&1 | grep -v pushed
sleep 50; $P press cross; sleep 6                                                      # the title
$P press down 0.2; sleep 1.5; $P press down 0.2; sleep 1.5; $P press cross; sleep 5     # New Game
$P press cross; sleep 4                                                                # friends' statistics: OK
$P press cross; sleep 4                                                                # Campaign
$P press up 0.2; sleep 1.5; $P press up 0.2; sleep 1.5; $P press cross; sleep 4         # Explorer
$P press up 0.2; sleep 1.5; $P press cross; sleep 25                                   # Yes; the movie (black first)
echo "== movie: $("$ADB" shell "grep -c 'Videodec2: decoder configured' $F/bb-probe.log" | tr -d '\r') decoder line(s)"
$P shot "$OUT/$tag-movie.png" > /dev/null
$P press options; sleep 3                                                              # pause
$P shot "$OUT/$tag-paused.png" > /dev/null
if [ -n "$keys" ]; then
    IFS=, read -r -a presses <<< "$keys"
    for k in "${presses[@]}"; do $P press "$k" 0.2; sleep 1.5; done
    $P shot "$OUT/$tag-menu.png" > /dev/null
fi
sleep "$pause"
before=$("$ADB" shell "grep -c 'Frame rate' $F/bb-probe.log" | tr -d '\r')
$P press cross; sleep 2                                                                # Continue
$P shot "$OUT/$tag-resumed.png" > /dev/null
hung=0; prev=$before
for i in 1 2 3 4 5 6 7; do
    sleep 10
    now=$("$ADB" shell "grep -c 'Frame rate' $F/bb-probe.log" | tr -d '\r')
    last=$("$ADB" shell "grep 'Frame rate' $F/bb-probe.log | tail -1" | tr -d '\r')
    echo "t=$((i * 10)) s: frame-rate lines $before -> $now; $last"
    if [ "$i" -ge 3 ] && [ "$now" = "$prev" ]; then hung=1; break; fi
    prev=$now
done
$P shot "$OUT/$tag-end.png" > /dev/null
if [ "$hung" = 1 ]; then
    echo "HUNG: no new frame-rate lines; taking the thread dump"
    $P dump
    "$ADB" shell "grep -A3 'name=SDLThread' $F/bb-probe.log | tail -3; grep -A2 'name=MovieDecodeThre' $F/bb-probe.log | tail -2" | cut -c1-150
else
    echo "no hang: frames kept coming after the pause"
fi
bash "$R/u3_ab.sh" stop "$tag" 2>&1 | tail -1
