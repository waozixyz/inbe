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

    # For wasm4 Makefile
    export WASM_CC="${pkgs.llvmPackages.clang-unwrapped}/bin/clang"
    export WASM_CXX="${pkgs.llvmPackages.clang-unwrapped}/bin/clang++"
    export WASM_OPT="${pkgs.binaryen}/bin/wasm-opt"

    # Native display dependencies for w4 run-native
    export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath (with pkgs; [
      libx11
      libxrandr
      libxinerama
      libxcursor
      libxi
      libxext
      libGL
      alsa-lib
      wayland
      libxkbcommon
    ])}:$LD_LIBRARY_PATH"
    
    echo "⚡ Pure C WASM-4 Toolchain Loaded (Unwrapped Clang Engine)!"
  '';
}
