#!/usr/bin/env bash

set -xEeuo pipefail

: "${1:?Please specify a test to run. 'ledger-baking' or 'ledger-wallet'}"
app="$1"
shift

: "${1:?Please specify a protocol to run. 'Babylon'}"
protocol="$1"
shift

fail() { "${___fail:?$1}"; }

root="$(git rev-parse --show-toplevel)"

tezos_baking_platform="${root}/nix/dep/tezos-baking-platform"
flextesa_dev="${root}/test/flextesa-dev.nix"

sandbox_args=""
case "$protocol" in
  Carthage)
    branch="carthagenet"
    sandbox_args+=" --protocol-kind=Carthage --protocol-hash=PsCARTHAGazKbHtnKfLzQg3kms52kSRpgnDY982a9oYsSXRLQEb"
    ;;
  *)
    fail "Protocol not known, use 'Carthage'"
    ;;
esac

: "${client_bin_root:="$(nix-build "$tezos_baking_platform" -A tezos.$branch.kit --no-out-link)/bin"}"
: "${test_bin_root:="$(nix-build "$flextesa_dev" -A kit --no-out-link)/bin"}"

echo
if [ "${ledger:-}" = "" ]; then
  ledger_client="$client_bin_root/tezos-client"
  regex_group='(ledger://[^\"]*)'
  [[ $($ledger_client -P 0 list connected ledgers) =~ \"$regex_group.*$regex_group.*$regex_group\" ]]
  if [ ${#BASH_REMATCH[@]} -eq 0 ]; then
    fail 'Unable to find a connected ledger. Is the ledger connected and open to the Wallet or Baking app?'
  fi

  ledger_uris=("${BASH_REMATCH[@]:1:3}")
  echo
  echo "Running tests for each of the following ledgers:" "${ledger_uris[@]}"
else
  echo "Using specified ledger: $ledger"
  ledger_uris=("$ledger")
fi

for uri in "${ledger_uris[@]}"; do
  echo
  echo
  echo ">>> RUNNING TEST WITH LEDGER \"$uri\""
  echo
  (
    export PATH="$client_bin_root:$PATH"
    "$test_bin_root/tezos-sandbox" "$app" "$uri" $sandbox_args "$@"
  )
done
