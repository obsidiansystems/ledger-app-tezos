## Tests

The tests for the ledger are split into that of two types. 1) The apdu tests and 2) the flextesa tests.

### APDU tests
APDU tests use the ledgerblue python app to send bytes directly to the ledger. 
They are split up into tests that run on the wallet app, and tests that run on the baking app. To execute them, simply run
the various shell scripts found in `test/apdu-tests/<baking/wallet>`

### Flextesa
These tests run a version of the tezos protocol in a small sandbox environment. It allows us to setup multiple accounts
and run various scenarios in order to ensure that the ledger behaves appropriately. They can be run by using `test/run-flextesa-tests` and by
following the prompts. 


#### Different sections of the ledger wallet test

- voting
- delegation
- transactions
- contracts
- manager
- batch-transactions

Within each section of test suite, there are multiple test cases. Each test case involves some sort of interaction with the ledger device, and
is run 2 times -- once to test the rejection of the ledger's prompt, and once to test the acceptance of the prompt.

To run only a specific section with no rejections, run:
```
./run-flextesa-tests ledger-wallet Carthage --only-test=<section> --no-rejections
```

#### Notes about development on flextesa
The code for the flextesa sandbox and for the ledger tests lie inside of the tezos source code packed in the `nix/dep/flextesa-dev` thunk. It is
important to note that this repo is simply a specific branch of the tezos source code, containing the latest, most up-to-date versions of the ledger tests.
More specifically, the tests are in `tezos/src/bin_sandbox/command_ledger_<wallet/baking>.ml`.

The code used to generate the `tezos-client` and `tezos-node` executables that the sandbox uses (flextesa is a wrapper around these executables)
lies in `nix/dep/tezos-baking-platform/tezos/<network>`.

*IMPORTANT*: it is imperative that upon packing the thunk for the flextesa-dev repo, that you also update the sha256 in `test/flextesa-dev.nix` or
else the `./test/run-flextesa-tests.sh` script will not work

#### Hacking on the tests
##### Thunks
The flextesa-dev repo is stored in an [obelisk](https://github.com/obsidiansystems/obelisk) thunk. To hack on the repo,
install the obelisk command line app, and run `ob thunk unpack nix/flextesa-dev`. This will fetch the repo so that you
can hack on it. When you have committed changes to that repo and pushed them upstream, simply run `ob thunk pack nix/flextesa-dev` to
pack the repo into thunk form

##### Setup
I have found that the best way to hack on the tests involve some sort of environement such as the following
```
 cd nix/dep/flextesa-dev
 nix-shell -p ocamlPackages.utop opam ocamlPackages.dune m4 pkgconfig gmp hidapi                   
 opam init --root=.                                                                                
 eval $(opam env)                                                                                  
 export PATH=_opam/bin:$PATH
 make build-dev-deps
 dune build src/bin_sandbox/main.exe
```
I also usually tinker with the `test/run-flextesa-tests.sh` so that it gets the flextesa executable from `nix/dep/flextesa-dev/_build/default/src/bin_sandbox/main.exe` 
so that I can build and execute the test suite at a much faster rate than would otherwise be possible.

##### Flextesa Dbg module
Add: `Dbg.on := true;` to the beginning of the run function. This will enable you to add useful statements such as:
```ocaml
Dbg.e EF.(wf "id_script %s: %s" parameter tmp) ;
```
for working on tests
