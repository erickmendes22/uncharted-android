#!/usr/bin/env bash
# Scripted session helpers on the G2 (Git Bash):
#   pad.sh start <env-file>      push env (+BB_PAD_FILE), clear the pad file, start the game
#   pad.sh press <buttons> [s]   hold buttons for s seconds (default 0.4), then release
#   pad.sh shot <png>            screenshot to a local file
#   pad.sh dump                  every thread's stack and guest state into the game's log
#                                (BB_DUMP_REQUEST, set by start; the game goes on)
#   pad.sh stop <log> <env>      force-stop, pull the log, push the given env back
set -euo pipefail
export MSYS_NO_PATHCONV=1
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
P=dev.unchartedg2.game
F=/sdcard/Android/data/$P/files
# The game autosaves during a run (a run once reached the next chapter): every run starts
# from the owner's saves and leaves them as they were (backup taken before the first run).
SAVES_BACKUP=${SAVES_BACKUP:-user-backup-20261020-1349}  # in files/, made by hand
RESTORE_SAVES="cp -r $F/$SAVES_BACKUP/savedata/. $F/user/savedata/"
# The same file as the game sees it (its files directory); polled every 2 s, then removed.
DUMP_REQUEST=/storage/emulated/0/Android/data/$P/files/dump-request
case "$1" in
start)
    if "$ADB" shell pidof $P >/dev/null; then echo "game running: not starting"; exit 1; fi
    # Saves that differ from the backup before a run: the owner played since it was taken
    # (2026-10-20: restoring the old backup would have lost a session). Back them up first.
    newer=$("$ADB" shell "cd $F/user/savedata && for f in \$(find . -type f); do cmp -s \$f $F/$SAVES_BACKUP/savedata/\$f || echo \$f; done" | tr -d '\r' | head -3)
    if [ -n "$newer" ]; then
        echo "saves differ from $SAVES_BACKUP (the owner played?): $newer - not starting" >&2
        exit 1
    fi
    tmp=$(mktemp)
    { cat "$2"; echo "BB_PAD_FILE=$F/pad.txt"; echo "BB_DUMP_REQUEST=$DUMP_REQUEST"; } > "$tmp"
    "$ADB" push "$(cygpath -w "$tmp")" $F/env.txt >/dev/null
    rm -f "$tmp"
    "$ADB" shell "echo > $F/pad.txt; rm -f $F/bb-probe.log $F/dump-request"
    # The screen off (owner away, 10 min timeout): the activity pauses at once and SDL waits
    # for a surface - the game hangs before the GPU starts. Wake it first (no lock screen).
    "$ADB" shell "input keyevent KEYCODE_WAKEUP; $RESTORE_SAVES; sleep 1"
    "$ADB" shell am start -n $P/.GameActivity >/dev/null
    echo started
    ;;
press)
    "$ADB" shell "echo $2 > $F/pad.txt; sleep ${3:-0.4}; echo > $F/pad.txt"
    ;;
shot)
    "$ADB" shell screencap -p /sdcard/bb-shot.png
    "$ADB" pull /sdcard/bb-shot.png "$(cygpath -w "$2")" >/dev/null
    "$ADB" shell rm -f /sdcard/bb-shot.png
    ;;
dump)
    # ~70 threads, 20 ms each, after a poll of up to 2 s.
    "$ADB" shell "touch $F/dump-request"
    sleep 6
    ;;
stop)
    "$ADB" shell am force-stop $P
    "$ADB" shell "$RESTORE_SAVES"
    "$ADB" pull $F/bb-probe.log "$(cygpath -w "$2")" >/dev/null || true
    "$ADB" push "$(cygpath -w "$3")" $F/env.txt >/dev/null
    "$ADB" shell "echo > $F/pad.txt"
    echo "stopped; env restored from $3"
    ;;
esac
