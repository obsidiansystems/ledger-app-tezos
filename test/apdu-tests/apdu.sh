#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

nix-shell "$root/nix/ledgerblue.nix" -A shell --pure --run 'LEDGER_PROXY_ADDRESS=127.0.0.1 LEDGER_PROXY_PORT=9999 python -m ledgerblue.runScript' 
#nix-shell "$root/nix/ledgerblue.nix" -A shell --pure --run 'python -m ledgerblue.runScript' 
## NOTE: This additional option to the python script is useful "--apdu" for debugging, but is turned
## off to make it easier for QA to test
