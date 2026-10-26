#!/usr/bin/env bash
set -euo pipefail
out=${1:?asset directory}
mkdir -p "$out/drivers"
file="$out/drivers/Turnip_Gen8_V37.zip"
curl --fail --location --retry 3 --output "$file" https://github.com/ducvd89/Bloodborne-Android/releases/download/turnip-android-gen8-26.3.0-9c7e022/Turnip_Gen8_V37.zip
echo "d0335542ec996fe33e83dfbec0d86aed301da95d22b671a88b40a4722d115b48  $file" | sha256sum --check --status
