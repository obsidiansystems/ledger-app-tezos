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

  bolos = {
    s = {
      sdk = fetchThunk nix/dep/nanos-secure-sdk;
      env = pkgs.callPackage nix/bolos-env.nix { clangVersion = 4; };
    };
    x = {
      sdk = /home/elliot/Downloads/nanox/sdk-nanox-1.2.4_dev3-rc2;
      env = pkgs.callPackage nix/bolos-env.nix { clangVersion = 7; };
    };
  };

  src = pkgs.lib.sources.sourceFilesBySuffices (pkgs.lib.sources.cleanSource ./.) [".c" ".h" ".gif" "Makefile"];

  build = target: commit:
    let
      bolosSdk = if target == "x" then bolos.x.sdk else bolos.s.sdk;
      bolosEnv = if target == "x" then bolos.x.env else bolos.s.env;
      app = bakingApp: pkgs.runCommand "ledger-app-tezos-${if bakingApp then "baking" else "wallet"}" {} ''
        set -Eeuo pipefail

        cp -a '${src}'/* .
        chmod -R u+w .

        export BOLOS_SDK='${bolosSdk}'
        export BOLOS_ENV='${bolosEnv}'

        echo "MARKO!"
        '${fhs}/bin/enter-fhs' <<EOF

        set -Eeuxo pipefail

        export BOLOS_SDK='${bolosSdk}'
        export BOLOS_ENV='${bolosEnv}'

        # For NanoX SDK Makefiles now require clang on the PATH.
        export PATH="$BOLOS_ENV/clang-arm-fropi/bin:$PATH"

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

  buildResult = target: if commit == null then throw "Set 'commit' attribute to git commit or use nix/build.sh instead" else build target commit;

  # The package clang-analyzer comes with a perl script `scan-build` that seems
  # to get quickly lost with the cross-compiler of the SDK if run by itself.
  # So this script reproduces what it does with fewer magic attempts:
  # * It prepares the SDK like for a normal build.
  # * It intercepts the calls to the compiler with the `CC` make-variable
  #   (pointing at `.../libexec/scan-build/ccc-analyzer`).
  # * The `CCC_*` variables are used to configure `ccc-analyzer`: output directory
  #   and which *real* compiler to call after doing the analysis.
  # * After the build an `index.html` file is created to point to the individual
  #   result pages.
  #
  # See
  # https://clang-analyzer.llvm.org/alpha_checks.html#clone_alpha_checkers
  # for the list of extra analyzers that are run.
  #
  runClangStaticAnalyzer =
     let
       interestingExtrasAnalyzers = [
         # "alpha.clone.CloneChecker" # this one is waaay too verbose
         "alpha.security.ArrayBound"
         "alpha.security.ArrayBoundV2"
         "alpha.security.MallocOverflow"
         # "alpha.security.MmapWriteExec" # errors as “not found” by ccc-analyzer
         "alpha.security.ReturnPtrRange"
         "alpha.security.taint.TaintPropagation"
         "alpha.deadcode.UnreachableCode"
         "alpha.core.CallAndMessageUnInitRefArg"
         "alpha.core.CastSize"
         "alpha.core.CastToStruct"
         "alpha.core.Conversion"
         # "alpha.core.FixedAddr" # Seems noisy, and about portability.
         "alpha.core.IdenticalExpr"
         "alpha.core.PointerArithm"
         "alpha.core.PointerSub"
         "alpha.core.SizeofPtr"
         # "alpha.core.StackAddressAsyncEscape" # Also not found
         "alpha.core.TestAfterDivZero"
         "alpha.unix.cstring.BufferOverlap"
         "alpha.unix.cstring.NotNullTerminated"
         "alpha.unix.cstring.OutOfBounds"
       ] ;
       analysisOptions =
          pkgs.lib.strings.concatMapStringsSep
             " "
             (x: "-analyzer-checker " + x)
             interestingExtrasAnalyzers ;
     in
     bakingApp:
        pkgs.runCommand
         "static-analysis-html-${if bakingApp then "baking" else "wallet"}" {} ''
            set -Eeuo pipefail
            mkdir -p $out
            cp -a '${src}'/* .
            chmod -R u+w .
            '${fhs}/bin/enter-fhs' <<EOF
            set -Eeuxo pipefail
            export BOLOS_SDK='${bolos.s.sdk}'
            export BOLOS_ENV='${bolos.s.env}'
            export COMMIT='${if commit == null then "TEST" else commit}'
            export CCC_ANALYZER_HTML="$out"
            export CCC_ANALYZER_OUTPUT_FORMAT=html
            export CCC_ANALYZER_ANALYSIS="${analysisOptions}"
            export CCC_CC='${bolos.s.env}/clang-arm-fropi/bin/clang'
            export CLANG='${bolos.s.env}/clang-arm-fropi/bin/clang'
            export APP='${if bakingApp then "tezos_baking" else "tezos_wallet"}'
            make clean
            make all CC=${pkgs.clangAnalyzer}/libexec/scan-build/ccc-analyzer
            EOF
            export outhtml="$out/index.html"
            echo "<html><title>Analyzer Report</title><body><h1>Clang Static Analyzer Results</h1>" > $outhtml
            printf "<p>App: <code>${if bakingApp then "tezos_baking" else "tezos_wallet"}</code></p>" >> $outhtml
            printf "<p>Date: $(date -R)</p>" >> $outhtml
            printf "<h2>File-results:</h2>" >> $outhtml
            for html in $(ls $out/report*.html) ; do
                echo "<p>" >> $outhtml
                printf "<code>" >> $outhtml
                cat $html | grep BUGFILE | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/' >> $outhtml
                printf "</code>" >> $outhtml
                printf "<code style=\"color: green\">+" >> $outhtml
                cat $html | grep BUGLINE | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/' >> $outhtml
                printf "</code><br/>" >> $outhtml
                cat $html | grep BUGDESC | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/' >> $outhtml
                printf " → <a href=\"./$(basename $html)\">full-report</a>" >> $outhtml
                echo "</p>" >> $outhtml
            done
            echo "</body></html>" >> $outhtml
          '';
in {
  nano = {
    s = buildResult "s";
    x = buildResult "x";
  };

  clangAnalysis = {
     baking = runClangStaticAnalyzer true;
     wallet = runClangStaticAnalyzer false;
  };

  env = {
    # Script that places you in the environment to run `make`, etc.
    shell = pkgs.writeScriptBin "env-shell" ''
      #!${pkgs.stdenv.shell}
      export BOLOS_SDK='${bolos.s.sdk}'
      export BOLOS_ENV='${bolos.s.env}'
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

    compiler = bolos.s.env;
    sdk = bolos.s.sdk;
  };
}
