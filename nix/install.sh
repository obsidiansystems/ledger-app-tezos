#!/usr/bin/env bash
set -Eeuxo pipefail

rootdir=$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )

: "${VERSION:=${2:-"$(git -C "$rootdir" describe --tags | cut -f1 -d- | cut -f2 -dv)"}}"

install-wallet() {
  "$rootdir/install.sh" 'Tezos Wallet' "$("$rootdir/nix/build.sh" -A wallet)/bin/app.hex" "$VERSION"
}
install-baking() {
  "$rootdir/install.sh" 'Tezos Baking' "$("$rootdir/nix/build.sh" -A baking)/bin/app.hex" "$VERSION"
}

export rootdir
export VERSION
export -f install-wallet
export -f install-baking

nix-shell "$rootdir/nix/ledgerblue.nix" -A shell --run "$(cat <<EOF
if [ "${1:-}" = "wallet" ]; then
  install-wallet
elif [ "${1:-}" = "baking" ]; then
  install-baking
else
  install-wallet
  install-baking
fi
EOF
)"
