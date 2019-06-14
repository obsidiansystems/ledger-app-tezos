#!/usr/bin/env bash
set -Eeuo pipefail

export target="${1:?Please specify target, either 's' for Nano S or 'x' for Nano X}"
shift

root="$(git rev-parse --show-toplevel)"
export root

install() {
  local release_file
  release_file=$("$root/nix/build.sh" -A "nano.${target}.release.$1")
  bash "$root/release-installer.sh" "$release_file"
}
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
