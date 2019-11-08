{ pkgs ? import (builtins.fetchTarball { url = "https://github.com/NixOS/nixpkgs/archive/9c9a249b0133c3cd30757c3594d18183689adc3f.tar.gz"; sha256 = "0g2wmys9qckz4awcp7img8672rhlhjhjq08ybgc0d4avgc7fkpw5"; }) {
  overlays = [ (self: super: {
    all-cabal-hashes = self.fetchurl {
      url = "https://github.com/commercialhaskell/all-cabal-hashes/archive/8ce20718874c5d1d258f436b0360cf04b6fd4b1e.tar.gz";
      sha256 = "1ipjha4dakiff1rf3v8fz9cs2c2n1hw0mma7j5lwk7cr82rqg58b";
    };
  })]; }
, compiler ? "ghc864"
, reflex-platform ? import (builtins.fetchTarball {
    url = "https://github.com/reflex-frp/reflex-platform/archive/9e306f72ed0dbcdccce30a4ba0eb37aa03cf91e3.tar.gz";
    sha256 = "1crlwfw6zsx2izwsd57i5651p5pfyrpkfjwp6fc1yd4q4d4n7g2m";
  }) {}
}:
let
 inherit (reflex-platform) hackGet;
in pkgs.haskell.packages.${compiler}.developPackage {
  name = "apdu-gen";
  root = ./.;
  source-overrides = {
    constraints-extras = "0.3.0.1";
    dependent-sum = "0.6.2.0";
    tezos-bake-monitor-lib = (hackGet ../../../nix/dep/tezos-bake-monitor-lib) + /tezos-bake-monitor-lib;
  };
}
