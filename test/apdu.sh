#!/usr/bin/env bash
set -eu

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$DIR"

if ! nix-shell ledger-blue-shell.nix --pure --run 'python -m ledgerblue.runScript --apdu'; then
    python -m ledgerblue.runScript --apdu
fi
