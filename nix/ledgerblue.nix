{ pkgs ? import ../nix/nixpkgs.nix {}, ... }:
rec {
  withLedgerblue = (pkgs.python36.withPackages (ps: with ps; [
    ecpy hidapi pycrypto python-u2flib-host requests ledgerblue pillow pkgs.hidapi protobuf
  ]));
  shell = withLedgerblue.env;
}
