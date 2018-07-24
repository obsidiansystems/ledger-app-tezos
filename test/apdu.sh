#!/usr/bin/env bash
set -eu

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$DIR"

nix-shell ledger-blue-shell.nix --pure --run 'python -m ledgerblue.runScript --apdu'
