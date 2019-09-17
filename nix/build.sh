#!/usr/bin/env bash
set -Eeuo pipefail

# Override package set by passing --arg pkgs

descr=$(git describe --tags --abbrev=8 --always --long --dirty 2>/dev/null)
echo >&2 "Git description: $descr"
exec nix-build --no-out-link --argstr gitDescribe "$descr" "$@" ${NIX_BUILD_ARGS:-}
