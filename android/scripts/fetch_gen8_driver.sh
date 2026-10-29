#!/usr/bin/env bash
set -euo pipefail
out=${1:?asset directory}
mkdir -p "$out/drivers"
file="$out/drivers/Turnip_Gen8_V38_Patched.zip"
curl --fail --location --retry 3 --output "$file" https://github.com/StevenMXZ/Adreno-Tools-Drivers/releases/download/v27.0.0-R1/Turnip_Gen8_V38_Patched.zip
echo "2bbb279a3da7680503a54a51a3051c7a4302c8fb2a089f8b183b4b5d6ba9ac17  $file" | sha256sum --check --status
