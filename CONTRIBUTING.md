# Contributing

## Hacking with [Nix](https://nixos.org/nix/)

The `nix/` folder contains helper scripts for working with the ledger via Nix.

### Developing
Use `nix/env.sh` to enter a shell where you can run `make` and it will just work. You can also pass a command instead, e.g. `nix/env.sh make clean`.

For development, use `nix/watch.sh make APP=<tezos_baking|tezos_wallet>` to incrementally build on every change. Be sure to `nix/env.sh make clean` if you start watching a different `APP`.

### Building
To do a full Nix build run `nix/build.sh`. You can pass `nix-build` arguments to this to build specific attributes, e.g. `nix/build.sh -A wallet`.

### Installing
`nix/install.sh` will install both the wallet and baking apps. Use `nix/install.sh baking` to install just the baking app or `nix/install.sh wallet` to install just the wallet.
