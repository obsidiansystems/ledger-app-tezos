#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"

cp -f "$(nix-build -A env.s.ide.config.vscode --no-out-link)" "vscode.code-workspace"
chmod +w "vscode.code-workspace"

bear="$(nix-build -E '(import ./nix/dep/nixpkgs {}).bear' --no-out-link)/bin/bear"
# c.f. https://github.com/rizsotto/Bear/issues/182
nix/env.sh s --run "export LANG=C.UTF-8; export LC_CTYPE=C.UTF-8; make clean && \"$bear\" make; make clean"
