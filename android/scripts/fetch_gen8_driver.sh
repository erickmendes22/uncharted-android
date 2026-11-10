#!/usr/bin/env bash
set -euo pipefail
out=${1:?asset directory}
mkdir -p "$out/drivers"
source=https://github.com/StevenMXZ/Adreno-Tools-Drivers/releases/download/v39
download() {
    local file=$1 expected=$2
    curl --fail --location --retry 3 --output "$out/drivers/$file" "$source/$file"
    printf '%s  %s\n' "$expected" "$out/drivers/$file" | sha256sum --check --status
}
# StevenMXZ 2026-11-07: Gen8 V39 patched (a8xx) and Mesa v27.0.0-R2 (a6xx/a7xx).
# Pin GitHub's asset digest, not a guessed checksum.
download Turnip_Gen8_V39_Patched.zip bd457b30f195cf93a334b5302c221b1b5cb59c79c485935decf95d560a38c8a2
download Turnip_v27.0.0-R2.zip 8d30be022582d0c03b1918c019540491d00439e14e9e5e1359fe0bb19004f781
