#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
nix-shell "$root/nix/ledgerblue.nix" -A shell --pure --run 'python -m ledgerblue.runScript --apdu'
