{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    tcl
    tclPackages.tclx
    tclPackages.tk
    tclPackages.tcllib
  ];

  shellHook = ''
  	echo "Tcl/tk environment loaded"
  '';
}
