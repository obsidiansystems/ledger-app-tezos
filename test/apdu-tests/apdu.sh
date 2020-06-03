#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
nix-shell "$root/nix/ledgerblue.nix" -A shell --pure --run 'python -m ledgerblue.runScript' 
## NOTE: This additional option to the python script is useful "--apdu" for debugging, but is turned
## off to make it easier for QA to test
