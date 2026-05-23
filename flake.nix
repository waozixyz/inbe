{
  description = "Raylib Android and desktop development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;

      mkPkgs = system: import nixpkgs {
        inherit system;
        config = {
          allowUnfree = true;
          allowUnsupportedSystem = true;
          android_sdk.accept_license = true;
        };
      };

      mkShell = system:
        let
          pkgs = mkPkgs system;
          aarch64Pkgs = pkgs.pkgsCross.aarch64-multiplatform;

          windowsCrossEnabled = system == "x86_64-linux";

          mingwPkgs = import nixpkgs {
            inherit system;
            crossSystem = { config = "x86_64-w64-mingw32"; };
            config = {
              allowUnfree = true;
              allowUnsupportedSystem = true;
            };
          };

          mcfgthreads = pkgs.pkgsCross.mingwW64.windows.mcfgthreads;

          androidComposition = pkgs.androidenv.composeAndroidPackages {
            cmdLineToolsVersion = "11.0";
            buildToolsVersions = [ "34.0.0" ];
            platformVersions = [ "34" ];
            includeNDK = true;
            ndkVersions = [ "25.1.8937393" ];
            cmakeVersions = [ "3.22.1" ];
          };

          sdk = androidComposition.androidsdk;
          ndkPath = "${sdk}/libexec/android-sdk/ndk/25.1.8937393";

          pkgConfigPath = pkgs.lib.makeSearchPath "lib/pkgconfig" [
            pkgs.SDL2.dev
            pkgs.libdrm.dev
            pkgs.libgbm
            pkgs.libglvnd.dev
          ];

          aarch64PkgConfigPath = pkgs.lib.makeSearchPath "lib/pkgconfig" [
            aarch64Pkgs.SDL2.dev
            aarch64Pkgs.libdrm.dev
            aarch64Pkgs.libgbm
            aarch64Pkgs.libglvnd.dev
          ];

          windowsTargetPkgs = pkgs.lib.optionals windowsCrossEnabled [
            mingwPkgs.buildPackages.gcc
            mingwPkgs.windows.mingw_w64
            mcfgthreads
          ];

          windowsProfile = pkgs.lib.optionalString windowsCrossEnabled ''
            # Windows cross-compilation
            export WIN_CC="x86_64-w64-mingw32-gcc"
            export WIN_CXX="x86_64-w64-mingw32-g++"
            export WIN_AR="x86_64-w64-mingw32-ar"
            export WIN_RANLIB="x86_64-w64-mingw32-ranlib"
            export WIN_WINDRES="x86_64-w64-mingw32-windres"
            export WIN_STRIP="x86_64-w64-mingw32-strip"

            export CC="$WIN_CC"
            export CXX="$WIN_CXX"
            export AR="$WIN_AR"
            export RANLIB="$WIN_RANLIB"
            export WINDRES="$WIN_WINDRES"

            # Fix for:
            #   ld.bfd: cannot find -lmcfgthread
            #
            # LIBRARY_PATH is important because your Makefile's printed link command
            # does not appear to include $LDFLAGS.
            export MCFGTHREADS="${mcfgthreads}"
            export CPATH="$MCFGTHREADS/include:$CPATH"
            export LIBRARY_PATH="$MCFGTHREADS/lib:$LIBRARY_PATH"
            export LDFLAGS="-L$MCFGTHREADS/lib $LDFLAGS"
          '';
        in
          (pkgs.buildFHSEnv {
            name = "ray-inbe-env";
            extraOutputsToInstall = [ "dev" ];

            targetPkgs = pkgs: with pkgs; [
              SDL2
              SDL2.dev
              cmake
              emscripten
              gcc
              gnumake
              gradle
              jdk17
              libdrm
              libdrm.dev
              libgbm
              libglvnd
              libglvnd.dev
              mesa
              ncurses
              ninja
              pkg-config
              zlib
              zip
              aarch64Pkgs.stdenv.cc
              aarch64Pkgs.SDL2
              aarch64Pkgs.SDL2.dev
              aarch64Pkgs.libdrm
              aarch64Pkgs.libdrm.dev
              aarch64Pkgs.libgbm
              aarch64Pkgs.libglvnd
              aarch64Pkgs.libglvnd.dev
            ] ++ windowsTargetPkgs;

            profile = ''
          export JAVA_HOME="${pkgs.jdk17.home}"

          export ANDROID_HOME="${sdk}/libexec/android-sdk"
          export ANDROID_SDK_ROOT="${sdk}/libexec/android-sdk"
          export ANDROID_NDK_ROOT="${ndkPath}"

          export PKG_CONFIG_PATH="/usr/lib64/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig:${pkgConfigPath}:$PKG_CONFIG_PATH"

          export RAY_PKGS="sdl2 libdrm gbm egl glesv2"
          export RAY_SDL_CFLAGS="$(pkg-config --cflags sdl2)"
          export RAY_SDL_LDLIBS="$(pkg-config --libs sdl2)"
          export RAY_GL_CFLAGS="$(pkg-config --cflags libdrm gbm egl glesv2)"
          export RAY_GL_LDLIBS="$(pkg-config --libs libdrm gbm egl glesv2)"
          export RAY_CFLAGS="$RAY_SDL_CFLAGS $RAY_GL_CFLAGS"
          export RAY_LDLIBS="$RAY_SDL_LDLIBS $RAY_GL_LDLIBS"
          export RAY_SDL_INCLUDE_DIR="$(pkg-config --variable=includedir sdl2 | sed 's,/SDL2$,,')"

          export RAY_RAYLIB_CONFIG="-DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0"

          export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

          export WEB_CC="emcc"
          export WEB_AR="emar"
          export WEB_RANLIB="emranlib"

          export AARCH64_CC="${aarch64Pkgs.stdenv.cc}/bin/${aarch64Pkgs.stdenv.cc.targetPrefix}cc"
          export AARCH64_AR="${aarch64Pkgs.stdenv.cc.bintools.bintools}/bin/${aarch64Pkgs.stdenv.cc.targetPrefix}ar"
          export AARCH64_PKG_CONFIG_PATH="${aarch64PkgConfigPath}"
          export AARCH64_RAY_SDL_CFLAGS="-D_GNU_SOURCE=1 -D_REENTRANT -I${aarch64Pkgs.SDL2.dev}/include -I${aarch64Pkgs.SDL2.dev}/include/SDL2"
          export AARCH64_RAY_SDL_LDLIBS="-L${aarch64Pkgs.SDL2}/lib -lSDL2"
          export AARCH64_RAY_GL_CFLAGS="-I${aarch64Pkgs.libdrm.dev}/include -I${aarch64Pkgs.libdrm.dev}/include/libdrm -I${aarch64Pkgs.libgbm}/include -I${aarch64Pkgs.libglvnd.dev}/include"
          export AARCH64_RAY_GL_LDLIBS="-L${aarch64Pkgs.libdrm}/lib -L${aarch64Pkgs.libgbm}/lib -L${aarch64Pkgs.libglvnd}/lib -ldrm -lgbm -lEGL -lGLESv2"
          export AARCH64_RAY_CFLAGS="$AARCH64_RAY_SDL_CFLAGS $AARCH64_RAY_GL_CFLAGS"
          export AARCH64_RAY_LDLIBS="$AARCH64_RAY_SDL_LDLIBS $AARCH64_RAY_GL_LDLIBS"
          export AARCH64_RAY_SDL_INCLUDE_DIR="${aarch64Pkgs.SDL2.dev}/include"

          ${windowsProfile}

          if [ ! -f droid/local.properties ]; then
            cat > droid/local.properties <<EOF
sdk.dir=$ANDROID_SDK_ROOT
ndk.dir=$ANDROID_NDK_ROOT
cmake.dir=$ANDROID_SDK_ROOT/cmake/3.22.1
EOF
          fi
        '';
          }).env;
    in {
      devShells = forAllSystems (system: {
        default = mkShell system;
      });
    };
}
