#!/usr/bin/env bash

set -uo pipefail

root="$(git rev-parse --show-toplevel)"

fail() { unset ___empty; : "${___empty:?$1}"; }

[ -z "${1:-}" ] && fail "No command given; try running $0 make"

watchdirs=("$root/default.nix" "$root/nix" "$root/Makefile" "$root/src")

inotifywait="$(nix-build '<nixpkgs>' -A inotify-tools --no-out-link)/bin/inotifywait"
while true; do
  "$root/nix/env.sh" <<EOF
    $@
EOF
  if ! "$inotifywait" -qre close_write "${watchdirs[@]}"; then
    fail "inotifywait failed"
  fi
  echo "----------------------"
  echo
done
