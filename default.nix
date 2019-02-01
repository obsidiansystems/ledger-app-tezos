{ pkgs ? import <nixpkgs> {}, commit, ... }:
let

  fetchThunk = p:
    if builtins.pathExists (p + /git.json)
      then pkgs.fetchgit { inherit (builtins.fromJSON (builtins.readFile (p + /git.json))) url rev sha256; }
    else if builtins.pathExists (p + /github.json)
      then pkgs.fetchFromGitHub { inherit (builtins.fromJSON (builtins.readFile (p + /github.json))) owner repo rev sha256; }
    else p;

  fhs = pkgs.callPackage nix/fhs.nix {};
  bolosEnv = pkgs.callPackage nix/bolos-env.nix {};
  bolosSdk = fetchThunk nix/dep/nanos-secure-sdk;
  src = pkgs.lib.sources.cleanSource ./.;

  app = bakingApp: pkgs.runCommand "ledger-app-tezos-${if bakingApp then "baking" else "wallet"}" {} ''
    set -Eeuo pipefail

    cp -a '${src}'/* .
    chmod -R u+w .

    '${fhs}/bin/enter-fhs' <<EOF
    set -Eeuxo pipefail
    export BOLOS_SDK='${bolosSdk}'
    export BOLOS_ENV='${bolosEnv}'
    export APP='${if bakingApp then "tezos_baking" else "tezos_wallet"}'
    export COMMIT='${commit}'
    make clean
    make all
    EOF

    mkdir -p "$out"
    cp -R bin "$out"
    cp -R debug "$out"

    echo
    echo ">>>> Application size: <<<<"
    '${pkgs.binutils-unwrapped}/bin/size' "$out/bin/app.elf"
  '';
in rec {
  wallet = app false;
  baking = app true;
  release = pkgs.runCommand "release.tar.gz" {} ''
    cp '${wallet}/bin/app.hex' wallet.hex
    cp '${baking}/bin/app.hex' baking.hex
    tar czf "$out" wallet.hex baking.hex
  '';

  # Script that places you in the environment to run `make`, etc.
  env-shell = pkgs.writeScriptBin "env-shell" ''
    #!${pkgs.stdenv.shell}
    export BOLOS_SDK='${bolosSdk}'
    export BOLOS_ENV='${bolosEnv}'
    export COMMIT='${commit}'
    exec '${fhs}/bin/enter-fhs'
  '';
}
