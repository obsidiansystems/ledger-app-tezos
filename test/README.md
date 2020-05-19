## Tests

The tests for the ledger are split into that of two types. 1) The apdu tests and 2) the flextesa tests.

#### APDU tests
APDU tests use the ledgerblue python app to send messages directly to the ledger. 

### Flextesa
These tests run a version of the tezos protocol in a small sandbox environment. It allows us to setup multiple accounts
and run various scenarios in order to ensure that the ledger behaves appropriately. They can be run by using `test/run-flextesa-tests` and 
following the prompts. 

#### Notes about development on flextesa
The code for the flextesa sandbox and for the ledger tests lie inside of the tezos source code packed in the `nix/dep/flextesa-dev` thunk. More specifically,
the tests are in `tezos/src/bin_sandbox/command_ledger_<wallet/baking>.ml`.
The code used to generate the `tezos-client` and `tezos-node` executables that the sandbox uses (flextesa is a wrapper around these executables)
lies in `nix/dep/tezos-baking-platform/tezos/<network>`.
