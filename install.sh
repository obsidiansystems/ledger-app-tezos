#!/bin/sh
set -eux

cd "$(dirname "$0")"/..

app_name=Tezos
if [ "${1:-}X" != X ]; then
    app_name="$1"
fi

ICONHEX="$(python nanos-secure-sdk/icon.py ledger-app/icon.gif hexbitmaponly 2>/dev/null || :)"
if ! [ -z "$ICONHEX" ]; then
    icon="--icon $ICONHEX"
else
    icon=
fi

set -x
python -m ledgerblue.loadApp \
    --appFlags 0x00 \
    --dataSize 0x80 \
    --tlv \
    --curve ed25519 \
    --curve secp256k1 \
    --curve prime256r1 \
    --targetId 0x31100003 \
    --delete \
    --path 44"'"/1729"'" \
    --fileName ledger-app/bin/app.hex \
    --appName "$app_name" \
    --appVersion 1.0.0 \
    $icon
