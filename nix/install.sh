#! /usr/bin/env nix-shell
#! nix-shell -i bash ./ledgerblue.nix -A shell

set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"

target="${1:?Please specify target, either 's' for Nano S or 'x' for Nano X}"
shift

case "$target" in
  s) ;; x) ;;
  *)
    >&2 echo "Target must either be 's' for Nano S or 'x' for Nano X"
    exit 1
esac

install() {
  local app=$1
  shift
  local release_file
  release_file=$("$root/nix/build.sh" -A "nano.$target.release.$app" "$@")
  bash "$root/release-installer.sh" "$release_file"
}

if [ $# -eq 0 ]; then
  install wallet "$@"
  install baking "$@"
else
  app="$1"
  shift
  install "$app" "$@"
fi
