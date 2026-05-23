{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
	buildInputs = with pkgs; [
		gcc
		gnumake
		xorg.libX11
	];

	shellHook = ''
		echo "================================"
		echo " Inner Breeze Build Environment "
		echo "================================"
		echo " make  - build inbe and inbefb"
	'';
}
