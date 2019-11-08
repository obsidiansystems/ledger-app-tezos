{ pkgs ? import (builtins.fetchTarball { url = "https://github.com/NixOS/nixpkgs/archive/9c9a249b0133c3cd30757c3594d18183689adc3f.tar.gz"; sha256 = "0g2wmys9qckz4awcp7img8672rhlhjhjq08ybgc0d4avgc7fkpw5"; }) {}

, haskellPackages ? pkgs.haskellPackages }:
let
  reflex-platform = import (pkgs.fetchFromGitHub {
    owner = "reflex-frp";
    repo = "reflex-platform";
    rev = "9e306f72ed0dbcdccce30a4ba0eb37aa03cf91e3";
    sha256 = "1crlwfw6zsx2izwsd57i5651p5pfyrpkfjwp6fc1yd4q4d4n7g2m";
  }) {};
 hackGet = reflex-platform.hackGet;

 in 
haskellPackages.developPackage {
  name = "apdu-gen";
  root = ./.;
  overrides = self: super: {
    tezos-bake-monitor-lib = self.callCabal2nix "tezos-bake-monitor-lib" ((hackGet ../../../nix/dep/tezos-bake-monitor-lib) + /tezos-bake-monitor-lib) {};
  };
}
