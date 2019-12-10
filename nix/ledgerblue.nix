{ pkgs ? import ../nix/dep/nixpkgs {}, ... }:
rec {
  withLedgerblue = (pkgs.python36.withPackages (ps: with ps; [
    ecpy hidapi pycrypto python-u2flib-host requests ledgerblue pillow pkgs.hidapi protobuf
  ]));
  shell = withLedgerblue.env;
}
