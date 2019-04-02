#!/usr/bin/env bash
set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

cp -f "$(nix-build "$root" -A env.ide.config.vscode --no-out-link)" "$root/vscode.code-workspace"
chmod +w "$root/vscode.code-workspace"
(cd "$root" && "nix/env.sh" 'make clean && bear make; make clean')
