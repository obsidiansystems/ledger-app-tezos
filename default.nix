{ pkgs ? import ./nix/dep/nixpkgs {}, gitDescribe ? "TEST-dirty", nanoXSdk ? null, ... }:
let
  fetchThunk = p:
    if builtins.pathExists (p + /git.json)
      then pkgs.fetchgit { inherit (builtins.fromJSON (builtins.readFile (p + /git.json))) url rev sha256; }
    else if builtins.pathExists (p + /github.json)
      then pkgs.fetchFromGitHub { inherit (builtins.fromJSON (builtins.readFile (p + /github.json))) owner repo rev sha256; }
    else p;

  targets =
    {
      s = rec {
        name = "s";
        sdk = fetchThunk ./nix/dep/nanos-secure-sdk;
        env = pkgs.callPackage ./nix/bolos-env.nix { clangVersion = 4; };
        target = "TARGET_NANOS";
        targetId = "0x31100004";
        iconHex = pkgs.runCommand "nano-s-icon-hex" {
          nativeBuildInputs = [ (pkgs.python.withPackages (ps: [ps.pillow])) ];
        } ''
          python ${sdk + /icon.py} '${icons/nano-s-tezos.gif}' hexbitmaponly > "$out"
        '';
        nvramDataSize = appDir: pkgs.runCommand "${name}-nvram-data-size" {} ''
          envram_data="0x$(cat  | grep _envram_data '${appDir + /debug/app.map}' | cut -f1 -d' ')"
          nvram_data="0x$(grep _nvram_data '${appDir + /debug/app.map}' | cut -f1 -d' ')"
          echo "$(($envram_data - $nvram_data))" > "$out"
        '';
      };
      x = rec {
        name = "x";
        sdk = if nanoXSdk == null
          then throw "No NanoX SDK"
          else assert builtins.typeOf nanoXSdk == "path"; nanoXSdk;
        env = pkgs.callPackage ./nix/bolos-env.nix { clangVersion = 7; };
        target = "TARGET_NANOX";
        targetId = "0x33000004";
        iconHex = pkgs.runCommand "${name}-icon-hex" {
          nativeBuildInputs = [ (pkgs.python3.withPackages (ps: [ps.pillow])) ];
        } ''
          python '${sdk + /icon3.py}' --hexbitmaponly '${icons/nano-x-tezos.gif}' > "$out"
        '';
        nvramDataSize = appDir: pkgs.runCommand "${name}-nvram-data-size" {} ''
          envram_data="0x$(cat  | grep _envram_data '${appDir + /debug/app.map}' | cut -f1 -d' ')"
          nvram_data="0x$(grep _nvram_data '${appDir + /debug/app.map}' | cut -f1 -d' ')"
          echo "$(($envram_data - $nvram_data))" > "$out"
        '';
      };
    };

  src = pkgs.lib.sources.sourceFilesBySuffices (pkgs.lib.sources.cleanSource ./.) [".c" ".h" ".gif" "Makefile"];

  build = bolos:
    let
      app = bakingApp: pkgs.stdenv.mkDerivation {
        name = "ledger-app-tezos-nano-${bolos.name}-${if bakingApp then "baking" else "wallet"}";
        inherit src;
        postConfigure = ''
          PATH="$BOLOS_ENV/clang-arm-fropi/bin:$PATH"
        '';
        nativeBuildInputs = [
          (pkgs.python3.withPackages (ps: [ps.pillow ps.ledgerblue]))
        ];
        TARGET = bolos.target;
        GIT_DESCRIBE = gitDescribe;
        BOLOS_SDK = bolos.sdk;
        BOLOS_ENV = bolos.env;
        makeFlags = [
          "APP=${if bakingApp then "tezos_baking" else "tezos_wallet"}"
        ];
        installPhase = ''
          mkdir -p $out
          cp -R bin $out
          cp -R debug $out

          echo
          echo ">>>> Application size: <<<<"
          size $out/bin/app.elf
        '';
      };
      mkRelease = short_name: name: appDir: pkgs.runCommand "${short_name}-nano-${bolos.name}-release-dir" {} ''
        mkdir -p "$out"

        cp '${appDir + /bin/app.hex}' "$out/app.hex"

        cat > "$out/app.manifest" <<EOF
        name='${name}'
        nvram_size=$(cat '${bolos.nvramDataSize appDir}')
        target='nano_${bolos.name}'
        target_id=${bolos.targetId}
        version=$(echo '${gitDescribe}' | cut -f1 -d- | cut -f2 -dv)
        icon_hex=$(cat '${bolos.iconHex}')
        EOF
      '';

      walletApp = app false;
      bakingApp = app true;
    in {
      wallet = walletApp;
      baking = bakingApp;

      release = rec {
        wallet = mkRelease "wallet" "Tezos Wallet" walletApp;
        baking = mkRelease "baking" "Tezos Baking" bakingApp;
        all = pkgs.runCommand "${bolos.name}-release.tar.gz" {} ''
          cp -r '${wallet}' wallet
          cp -r '${baking}' baking
          cp '${./release-installer.sh}' install.sh
          chmod +x install.sh
          tar czf "$out" install.sh wallet baking
        '';
      };
    };

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
       ];
       analysisOptions =
          pkgs.lib.strings.concatMapStringsSep
             " "
             (x: "-analyzer-checker " + x)
             interestingExtrasAnalyzers;
     in bakingApp: bolos: ((build bolos).${if bakingApp then "baking" else "wallet"}).overrideAttrs (old: {
       CCC_ANALYZER_HTML = "${placeholder "out"}";
       CCC_ANALYZER_OUTPUT_FORMAT = "html";
       CCC_ANALYZER_ANALYSIS = analysisOptions;
       CCC_CC = "${bolos.env}/clang-arm-fropi/bin/clang";
       CLANG = "${bolos.env}/clang-arm-fropi/bin/clang";
       preBuild = ''
         mkdir -p $out
       '';
       makeFlags = old.makeFlags or []
         ++ [ "CC=${pkgs.clangAnalyzer}/libexec/scan-build/ccc-analyzer" ];
       installPhase = ''
        {
          echo "<html><title>Analyzer Report</title><body><h1>Clang Static Analyzer Results</h1>"
          printf "<p>App: <code>${if bakingApp then "tezos_baking" else "tezos_wallet"}</code></p>"
          printf "<h2>File-results:</h2>"
          for html in "$out"/report*.html ; do
            echo "<p>"
            printf "<code>"
            grep BUGFILE "$html" | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/'
            printf "</code>"
            printf "<code style=\"color: green\">+"
            grep BUGLINE "$html" | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/'
            printf "</code><br/>"
            grep BUGDESC "$html" | sed 's/^<!-- [A-Z]* \(.*\) -->$/\1/'
            printf " → <a href=\"./%s\">full-report</a>" "$(basename "$html")"
            echo "</p>"
          done
          echo "</body></html>"
        } > "$out/index.html"
      '';
     });

  mkTargets = mk: {
    s = mk targets.s;
    x = mk targets.x;
  };
in rec {
  nano = mkTargets build;

  wallet = {
    s = nano.s.wallet;
    x = nano.x.wallet;
  };
  baking = {
    s = nano.s.baking;
    x = nano.x.baking;
  };

  clangAnalysis = mkTargets (bolos: {
    baking = runClangStaticAnalyzer true bolos;
    wallet = runClangStaticAnalyzer false bolos;
  });

  env = mkTargets (bolos: {
    ide = {
      config = {
        vscode = pkgs.writeText "vscode-nano-${bolos.name}.code-workspace" (builtins.toJSON {
          folders = [ { path = "."; } ];
          settings = {
            "clangd.path" = pkgs.llvmPackages.clang-unwrapped + /bin/clangd;
          };
        });
      };
    };

    inherit (bolos.env) clang gcc;
    inherit (bolos) sdk;
  });
}
