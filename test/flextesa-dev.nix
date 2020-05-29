{}:

with import ../nix/dep/tezos-baking-platform {};

tezos.mkWithSrc {
  net = /master;
  src = ../nix/dep/flextesa-dev;
  sha256 = "1xqvggjz1sp6w89km3yrl6h6q8aqdknr07dr4aba4d1pb4r6b1py";
}
