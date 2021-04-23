# Contributing

## Hacking with [Nix](https://nixos.org/nix/)

The `nix/` folder contains helper scripts for working with the ledger via Nix.

### Installing
`nix/install.sh` will install both the wallet and baking apps. Use
`nix/install.sh s baking` to install just the baking app or
`nix/install.sh s wallet` to install just the wallet.

### Developing
Use `nix/env.sh <s or x>` to enter a shell where you can run `make` and it will just work. You can also pass a command instead, e.g. `nix/env.sh s --run "make clean SHELL=bash"`. All `make` commands should be prefixed with `SHELL=bash`. Check the Makefile to see what options exist

For development, use `nix/watch.sh s make APP=<tezos_baking|tezos_wallet>` to incrementally build on every change. Be sure to `nix/env.sh s --run "make clean SHELL=bash"` if you start watching a different `APP`.

#### Debugging
Set `DEBUG=1` in the `Makefile` to so that the user-defined `parse_error()` macro provides a line-number. For `printf` style debugging see [Ledger's official instructions](https://ledger.readthedocs.io/en/latest/userspace/debugging.html)

### Building
To do a full Nix build run `nix/build.sh`. You can pass `nix-build` arguments to this to build specific attributes, e.g. `nix/build.sh -A nano.s.wallet`.

### Using tezos-client
Set environment variable `TEZOS_LOG="client.signer.ledger -> debug"` when running tezos-client to get the byte-level IO being sent
directly to/from the ledger


### Editor Integration

#### Visual Studio Code

  1. Install `llvm-vs-code-extensions.vscode-clangd` extension.
  2. Run `nix/setup-vscode.sh` to create local configuration. Note that these files are non-relocatable so you need to rerun this if you move the directory.
  3. Restart Visual Studio Code.

### Releasing

`nix/build.sh -A nano.s.release.all`

`nix/build.sh -A nano.x.release.all`

### Notes on testing

See `test/README.md`
