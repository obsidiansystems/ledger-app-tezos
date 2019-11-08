{}:

with import ../nix/dep/tezos-baking-platform {};

tezos.mkWithSrc {
  net = /master;
  src = ../nix/dep/flextesa-dev;
  sha256 = "1kcsqp6wh1ipddychkdjsh3iw6xnkvymasbysxxhpr2yh5lis9jn";
}
