{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    llvmPackages.clang-unwrapped # Bypasses the hardening flags wrapper
    lld                          # The webassembly-capable linker
    wabt                         # WebAssembly tools
    binaryen                     # For wasm-opt sizing compression
  ];

  shellHook = ''
    # Point to the raw binary directly, escaping the Nix wrapper script
    export CC_UNWRAPPED="${pkgs.llvmPackages.clang-unwrapped}/bin/clang"
    export CXX_UNWRAPPED="${pkgs.llvmPackages.clang-unwrapped}/bin/clang++"

    # Native display dependencies for w4 run-native
    export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath (with pkgs; [
      xorg.libX11
      xorg.libXrandr
      xorg.libXinerama
      xorg.libXcursor
      xorg.libXi
      xorg.libXext
      libGL
      alsa-lib
      wayland
      libxkbcommon
    ])}:$LD_LIBRARY_PATH"
    
    echo "⚡ Pure C WASM-4 Toolchain Loaded (Unwrapped Clang Engine)!"
  '';
}