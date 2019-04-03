{ pkgs ? import nix/nixpkgs.nix {}, commit ? null, ... }:
let

  fetchThunk = p:
    if builtins.pathExists (p + /git.json)
      then pkgs.fetchgit { inherit (builtins.fromJSON (builtins.readFile (p + /git.json))) url rev sha256; }
    else if builtins.pathExists (p + /github.json)
      then pkgs.fetchFromGitHub { inherit (builtins.fromJSON (builtins.readFile (p + /github.json))) owner repo rev sha256; }
    else p;

  fhsWith = runScript: pkgs.callPackage nix/fhs.nix { inherit runScript; };
  fhs = fhsWith "bash";
  bolosEnv = pkgs.callPackage nix/bolos-env.nix {};
  bolosSdk = fetchThunk nix/dep/nanos-secure-sdk;
  src = pkgs.lib.sources.sourceFilesBySuffices (pkgs.lib.sources.cleanSource ./.) [".c" ".h" ".gif" "Makefile"];

  build = commit:
    let
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

      mkRelease = short_name: name: appDir: pkgs.runCommand "${short_name}-release-dir" {} ''
        mkdir -p "$out"

        cp '${appDir + /bin/app.hex}' "$out/app.hex"

        cat > "$out/app.manifest" <<EOF
        name='${name}'
        nvram_size=$(grep _nvram_data_size '${appDir + /debug/app.map}' | tr -s ' ' | cut -f2 -d' ')
        target_id=0x31100004
        version=$(echo '${commit}' | cut -f1 -d- | cut -f2 -dv)
        EOF

        cp '${dist/icon.hex}' "$out/icon.hex"
      '';

      walletApp = app false;
      bakingApp = app true;

    in {
      wallet = walletApp;
      baking = bakingApp;

      release = rec {
        wallet = mkRelease "wallet" "Tezos Wallet" walletApp;
        baking = mkRelease "baking" "Tezos Baking" bakingApp;
        all = pkgs.runCommand "release.tar.gz" {} ''
          cp -r '${wallet}' wallet
          cp -r '${baking}' baking
          cp '${./release-installer.sh}' install.sh
          chmod +x install.sh
          tar czf "$out" install.sh wallet baking
        '';
      };
    };

  buildResult = if commit == null then throw "Set 'commit' attribute to git commit or use nix/build.sh instead" else build commit;
in {
  inherit (buildResult) wallet baking release;

  env = {
    # Script that places you in the environment to run `make`, etc.
    shell = pkgs.writeScriptBin "env-shell" ''
      #!${pkgs.stdenv.shell}
      export BOLOS_SDK='${bolosSdk}'
      export BOLOS_ENV='${bolosEnv}'
      export COMMIT='${if commit == null then "TEST" else commit}'
      exec '${fhs}/bin/enter-fhs'
    '';

    ide = {
      config = {
        vscode = pkgs.writeText "vscode.code-workspace" (builtins.toJSON {
          folders = [ { path = "."; } ];
          settings = {
            "clangd.path" = fhsWith (pkgs.llvmPackages.clang-unwrapped + /bin/clangd) + /bin/enter-fhs;
          };
        });
      };
    };

    compiler = bolosEnv;
    sdk = bolosSdk;
  };
}
