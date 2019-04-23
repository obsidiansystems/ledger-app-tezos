#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"

cp -f "$(nix-build -A env.ide.config.vscode --no-out-link)" "vscode.code-workspace"
chmod +w "vscode.code-workspace"

cd "$root"
bear="$(nix-build -E '(import nix/nixpkgs.nix {}).bear' --no-out-link)/bin/bear"
nix/env.sh "make clean && \"$bear\" make; make clean"
