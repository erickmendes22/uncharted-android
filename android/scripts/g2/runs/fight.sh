#!/system/bin/sh
# fight.sh <secs>: on the G2, a crude brawl through BB_PAD_FILE for U3's pub fight (u3_ab.sh
# fight pushes and runs it). The owner (2026-10-23): a square attack first is countered by
# script; approach the thug, dodge his attack with triangle, and punch (square) only once his
# stance has changed. So each beat: walk forward 1 s, triangle presses for 2.5 s (to catch his
# attack), then three squares. Circle bursts now and then for grab escapes, a camera turn now
# and then. Blind input: the first two rooms are fought through, the washroom's big thug is not.
P=/sdcard/Android/data/dev.unchartedg2.game/files/pad.txt
end=$(( $(date +%s) + $1 )); i=0
while [ $(date +%s) -lt $end ]; do
    i=$((i + 1))
    if [ $((i % 10)) -eq 0 ]; then echo "rx=255" > $P; sleep 0.5; echo > $P; sleep 0.3
    elif [ $((i % 5)) -eq 0 ]; then for k in 1 2 3 4 5 6; do echo circle > $P; sleep 0.1; echo > $P; sleep 0.1; done
    else
        echo "ly=0" > $P; sleep 1.0; echo > $P; sleep 0.1
        for k in 1 2 3 4 5 6 7; do echo triangle > $P; sleep 0.15; echo > $P; sleep 0.2; done
        for k in 1 2 3; do echo square > $P; sleep 0.15; echo > $P; sleep 0.25; done
        sleep 0.3
    fi
done
echo > $P
