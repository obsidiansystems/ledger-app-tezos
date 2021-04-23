#!/usr/bin/env bash

set -uo pipefail

fail() { unset ___empty; : "${___empty:?$1}"; }

target="${1:?Please specify target, either 's' for Nano S or 'x' for Nano X}"
shift

[ -z "${1:-}" ] && fail "No command given; try running $0 make"

root="$(git rev-parse --show-toplevel)"

watchdirs=("$root/default.nix" "$root/nix" "$root/Makefile" "$root/src" "$root/icons")

inotifywait="$(nix-build "$root/nix/dep/nixpkgs" -A inotify-tools --no-out-link)/bin/inotifywait"
while true; do
  "$root/nix/env.sh" "$target" "--run" "$@"
  if ! "$inotifywait" -qre close_write "${watchdirs[@]}"; then
    fail "inotifywait failed"
  fi
  echo "----------------------"
  echo
done
