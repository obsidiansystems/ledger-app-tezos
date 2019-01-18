{ pkgs, ... }:
let
  libtinfo5 = pkgs.runCommand "libtinfo5" {} ''
    mkdir -p "$out/lib"
    ln -s '${pkgs.ncurses5}/lib/libncursesw.so.5' "$out/lib/libtinfo.so.5"
  '';
in pkgs.buildFHSUserEnv {
  name = "enter-fhs";

  # TODO: Reduce this set to the minimal set
  targetPkgs = pkgs: with pkgs; [
    alsaLib atk cairo cups dbus expat file fontconfig freetype gdb git glib
    libnotify libxml2 libxslt
    netcat nspr nss strace udev watch wget which xorg.libX11
    xorg.libXScrnSaver xorg.libXcomposite xorg.libXcursor xorg.libXdamage
    xorg.libXext xorg.libXfixes xorg.libXi xorg.libXrandr xorg.libXrender
    xorg.libXtst xorg.libxcb xorg.xcbutilkeysyms zlib zsh
    gnumake libtinfo5 glibc_multi.dev (python2.withPackages (ps: [ps.pillow]))
  ];
  runScript = "bash"; # "$SHELL";
}
