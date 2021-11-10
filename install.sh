#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

app_name=Tezos
if [ "${1:-}X" != X ]; then
    app_name="$1"
fi

app_dir="$root"
if [ "${2:-}X" != X ]; then
    app_dir="$2"
fi

if [ "${3:-}X" = X ]; then
    version="$(git -C "$root" describe --tags | cut -f1 -d- | cut -f2 -dv)"
else
    version="$3"
fi

set -x
python -m ledgerblue.loadApp \
    --appFlags 0x00 \
    --dataSize "$(grep _nvram_data_size "$app_dir/debug/app.map" | tr -s ' ' | cut -f2 -d' ')" \
    --tlv \
    --curve ed25519 \
    --curve secp256k1 \
    --curve secp256r1 \
    --targetId "${TARGET_ID:-0x31100004}" \
    --delete \
    --path 44"'"/1729"'" \
    --fileName "$app_dir/bin/app.hex" \
    --appName "$app_name" \
    --appVersion "$version" \
    --icon "$(cat "$root/dist/icon.hex")" \
    --targetVersion ""
