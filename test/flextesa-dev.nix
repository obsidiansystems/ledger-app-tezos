{}:

with import ../nix/dep/tezos-baking-platform {};

tezos.mkWithSrc {
  net = /master;
  src = ../nix/dep/flextesa-dev;
  sha256 = "1yy7h7nxkzl6bdjc37nwn6p7cx19qiy29cg6pcvirqfrnqkav6rb";
}
