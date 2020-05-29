#!/usr/bin/env bash
set -euo pipefail

root="$(git rev-parse --show-toplevel)"

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

{
  echo 8002000311048000002c800006c18000000080000000 
} | nix-shell "$root/nix/ledgerblue.nix" -A shell --pure --run 'python -m ledgerblue.runScript --apdu'
