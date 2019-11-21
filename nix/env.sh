#!/usr/bin/env bash

target="${1:?Please specify target, either 's' for Nano S or 'x' for Nano X}"
shift

root="$(git rev-parse --show-toplevel)"

if [ $# -eq 0 ]; then
  exec nix-shell "$root" -A "nano.${target}.wallet"
else
  exec nix-shell "$root" -A "nano.${target}.wallet" "$@"
fi
