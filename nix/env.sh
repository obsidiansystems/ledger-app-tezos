#!/usr/bin/env bash

commit=$(git describe --abbrev=8 --always 2>/dev/null)
echo >&2 "Git commit: $commit"
shell_dir="$(nix-build -A env-shell --no-out-link --argstr commit "$commit" "${NIX_BUILD_ARGS:-}")"
shell="$shell_dir/bin/env-shell"

if [ $# -eq 0 ]; then
  echo >&2 "Entering via $shell"
  exec "$shell"
else
  exec "$shell" <<EOF
    $@
EOF
fi
