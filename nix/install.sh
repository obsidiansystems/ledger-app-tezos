#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

: "${VERSION:=${2:-"$(git -C "$root" describe --tags | cut -f1 -d- | cut -f2 -dv)"}}"

install-wallet() {
  "$root/install.sh" 'Tezos Wallet' "$("$root/nix/build.sh" -A wallet)" "$VERSION"
}
install-baking() {
  "$root/install.sh" 'Tezos Baking' "$("$root/nix/build.sh" -A baking)" "$VERSION"
}

export root
export VERSION
export -f install-wallet
export -f install-baking

nix-shell "$root/nix/ledgerblue.nix" -A shell --run "$(cat <<EOF
set -Eeuo pipefail
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
