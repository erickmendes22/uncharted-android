#!/usr/bin/env bash
set -euo pipefail
out=${1:?asset directory}
mkdir -p "$out/art"
curl --fail --location --retry 3 --output "$out/art/collection.png" 'https://image.api.playstation.com/vulcan/img/rnd/202011/1101/zC3kulm2Ho3ZE38ZUYHW7WMK.png'
echo "6db64158a2c72030415641dd786444fd58e90a5a6978376043cf70bcd9fdbf57  $out/art/collection.png" | sha256sum --check --status
curl --fail --location --retry 3 --output "$out/art/hero.png" 'https://image.api.playstation.com/vulcan/img/rnd/202011/1101/6JXslFDceEZUjTfwEamrXFTJ.png'
echo "23dec714b391171df804cab7e2d1e03caa450cb8095d3065ccd1eb80a28fc2e0  $out/art/hero.png" | sha256sum --check --status
