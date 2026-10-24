#!/usr/bin/env bash
# Start U1 with <env-file> (+BB_PAD_FILE) and reach gameplay from the owner's "Continue" save:
# collection title -> U1 -> U1 title -> Continue -> network dialog OK -> level.
#   nav.sh <env-file> <screenshot-prefix>
set -euo pipefail
S=$(dirname "$0")
bash "$S/pad.sh" start "$1"
sleep 50; bash "$S/pad.sh" press cross   # collection title
sleep 8;  bash "$S/pad.sh" press cross   # Uncharted: Drake's Fortune
sleep 25; bash "$S/pad.sh" press cross   # U1 title
sleep 6;  bash "$S/pad.sh" shot "$2-menu.png"
bash "$S/pad.sh" press cross             # Continue (first item)
sleep 30; bash "$S/pad.sh" press cross   # network dialog: OK
sleep 15; bash "$S/pad.sh" press cross   # in case the dialog came later (loading ignores it)
sleep 60; bash "$S/pad.sh" shot "$2-game.png"
echo "in game (check $2-game.png)"
