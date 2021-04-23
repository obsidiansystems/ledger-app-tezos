#!/usr/bin/env bash
set -Eeuo pipefail
: "${1:?Please specify one or more directories or .tar.gz files; they should contain an 'app.manifest'}"

for arg in "$@"; do
  echo

  app_dir=""
  if [ -d "$arg" ]; then
    app_dir="$arg"
  else
    app_dir=$(tmp_dir=$(mktemp -d) && tar xf "$arg" -C "$tmp_dir" && echo "$tmp_dir")
    echo "App for $arg unpacked in $app_dir"
  fi

  source "$app_dir/app.manifest"

  echo "Installing ${name:?manifest file is missing field}" \
    "version ${version:?manifest file is missing field}"
  echo

  set -x

  appFlag="0x00"
  if [ $target == "nano_x" ]; then
    appFlag="0x240"
  fi

  python -m ledgerblue.loadApp \
    --appFlags "$appFlag" \
    --dataSize "${nvram_size:?manifest file is missing field}" \
    --tlv \
    --curve ed25519 \
    --curve secp256k1 \
    --curve prime256r1 \
    --targetId "${target_id:?manifest file is missing field}" \
    --delete \
    --path "44'/1729'" \
    --fileName "$app_dir/app.hex" \
    --appName "$name" \
    --appVersion "$version" \
    --icon "${icon_hex:?manifest file is missing field}" \
    --targetVersion ""
done
