{
  description = "Raylib Android and desktop development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs {
        inherit system;
        config = {
          allowUnfree = true;
          android_sdk.accept_license = true;
        };
      };

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
    in {
      devShells.${system}.default = (pkgs.buildFHSEnv {
        name = "ray-inbe-env";
        extraOutputsToInstall = [ "dev" ];

        targetPkgs = pkgs: with pkgs; [
          SDL2
          SDL2.dev
          cmake
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
        ];

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


          if [ ! -f local.properties ] && \
             { [ -f settings.gradle ] || [ -f settings.gradle.kts ] || [ -f build.gradle ] || [ -f build.gradle.kts ]; }; then
            cat > local.properties <<EOF
sdk.dir=$ANDROID_SDK_ROOT
ndk.dir=$ANDROID_NDK_ROOT
cmake.dir=$ANDROID_SDK_ROOT/cmake/3.22.1
EOF
          fi
        '';
      }).env;
    };
}
