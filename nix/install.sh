#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

install() {
  local release_file
  release_file=$("$root/nix/build.sh" -A "release.$1")
  bash "$root/release-installer.sh" "$release_file"
}

export root
export -f install

nix-shell "$root/nix/ledgerblue.nix" -A shell --run "$(cat <<EOF
set -Eeuo pipefail
if [ $# -eq 0 ]; then
  install wallet
  install baking
else
  install "${1:-}"
fi
EOF
)"
