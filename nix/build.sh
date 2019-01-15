#!/usr/bin/env bash
set -Eeuo pipefail

# Override package set by passing --arg pkgs

commit=$(git describe --abbrev=8 --always 2>/dev/null)
echo >&2 "Git commit: $commit"
exec nix-build --no-out-link --argstr commit "$commit" "$@"