let
  # TODO: Upstream this change and use ../nixpkgs.nix as the pin.
  pkgs = import ((import <nixpkgs> {}).fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs";
    rev = "e3be0e49f082df2e653d364adf91b835224923d9";
    sha256 = "15xcs10fdc3lxvasd7cszd6a6vip1hs9r9r1qz2z0n9zkkfv3rrq";
  }) {};
in pkgs.runCommand "ledger-blue-shell" {
  buildInputs = with pkgs.python36Packages; [ ecpy hidapi pycrypto python-u2flib-host requests ledgerblue pillow pkgs.hidapi ];
} ""
