#!/usr/bin/env bash

target="${1:?Please specify target, either 's' for Nano S or 'x' for Nano X}"
shift

root="$(git rev-parse --show-toplevel)"

shell="$(nix-build "$root" -A "env.${target}.shell" --no-out-link ${NIX_BUILD_ARGS:-})/bin/env-shell"

if [ $# -eq 0 ]; then
  echo >&2 "Entering via $shell"
  exec "$shell"
else
  exec "$shell" <<EOF
    $@
EOF
fi
