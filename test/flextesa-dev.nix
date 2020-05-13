{}:

with import ../nix/dep/tezos-baking-platform {};

tezos.mkWithSrc {
  net = /master;
  src = ../nix/dep/flextesa-dev;
  sha256 = "175rjg3yi6p08j31190xamdk0qwn3jyramvpzj52l9v3adc0pkvh";
}
