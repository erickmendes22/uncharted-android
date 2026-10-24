#!/usr/bin/env bash
# Guest code inventory: extraction -> XED -> checking Remill support.
# Run in WSL: bash /mnt/c/repos/uncharted-g2/android/scripts/inventory/run.sh [game folder]
# Everything derived from the game goes to ~/src/inventory (outside git); the summary may go to notes/.
set -euo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
GAME=${1:-/mnt/d/Games/UnchartedPC/Game}
W=${INVENTORY_DIR:-$HOME/src/inventory}
DEPS=$HOME/src/remill/dependencies/install
mkdir -p "$W/code" "$W/xed"
cd "$W"

[[ -f code/summary.json ]] || python3 -I "$HERE/extract_code.py" "$GAME" code
clang-22 -O2 -I"$DEPS/include" "$HERE/xed_inventory.c" "$DEPS/lib/libxed.a" -o xed_inventory

for json in code/*.json; do
    name=$(basename "$json" .json)
    [[ $name == summary ]] && continue
    python3 -I - "$json" <<'EOF' | while read -r file vaddr; do
import json, sys
for s in json.load(open(sys.argv[1]))['segments']:
    print(s['file'], s['vaddr'])
EOF
        ./xed_inventory "$name" "code/$file" "$vaddr" "code/$name.ranges" "xed/$name"
    done
done
python3 -I "$HERE/remill_coverage.py" "$W"
