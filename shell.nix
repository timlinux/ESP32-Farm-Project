with import <nixpkgs> { };

let
  # For packages pinned to a specific version
  pinnedHash = "3dc440faeee9e889fe2d1b4d25ad0f430d449356"; 
  pinnedPkgs = import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/${pinnedHash}.tar.gz") { };
in pkgs.mkShell rec {
  name = "arduinoEnv";
  buildInputs = [
    pinnedPkgs.fritzing
    pinnedPkgs.arduino
    pinnedPkgs.arduino-cli
  ];

  shellHook = ''
  '';
  postShellHook = ''
  '';

}
