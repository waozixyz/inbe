{
  description = "Waozi raylib cross-build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixpkgs-sdl2.url = "github:NixOS/nixpkgs/nixos-24.11";
  };

  outputs = { self, nixpkgs, nixpkgs-sdl2 }:
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
          sdl2Pkgs = import nixpkgs-sdl2 {
            inherit system;
            config = {
              allowUnfree = true;
              allowUnsupportedSystem = true;
            };
          };
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

          mingw32Pkgs = import nixpkgs {
            inherit system;
            crossSystem = { config = "i686-w64-mingw32"; };
            config = {
              allowUnfree = true;
              allowUnsupportedSystem = true;
            };
          };

          mcfgthreads = pkgs.pkgsCross.mingwW64.windows.mcfgthreads;
          mcfgthreads32 = pkgs.pkgsCross.mingw32.windows.mcfgthreads;

          androidComposition = pkgs.androidenv.composeAndroidPackages {
            cmdLineToolsVersion = "11.0";
            buildToolsVersions = [ "34.0.0" ];
            platformVersions = [ "34" "35" ];
            includeNDK = true;
            ndkVersions = [ "28.2.13676358" ];
            cmakeVersions = [ "3.22.1" ];
            includeEmulator = true;
            includeSystemImages = true;
            systemImageTypes = [ "google_apis_playstore" "default" ];
            abiVersions = [ "arm64-v8a" "x86_64" ];
          };

          sdk = androidComposition.androidsdk;
          ndkPath = "${sdk}/libexec/android-sdk/ndk-bundle";

          pkgConfigPath = pkgs.lib.makeSearchPath "lib/pkgconfig" [
            sdl2Pkgs.SDL2.dev
            pkgs.curl.dev
            pkgs.openssl.dev
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
            mingw32Pkgs.buildPackages.gcc
            mingw32Pkgs.windows.mingw_w64
            mcfgthreads32
          ];

          linuxdeployPluginAppImage = pkgs.stdenvNoCC.mkDerivation {
            pname = "linuxdeploy-plugin-appimage";
            version = "1-alpha-20250213-1";

            src = pkgs.fetchurl {
              url = "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-appimage-x86_64.AppImage";
              hash = "sha256-mS1QKiSOFKsYVEjd9vbn0lVYy4TUYjw1TDrzUMJfzLM=";
            };

            dontUnpack = true;

            installPhase = ''
              runHook preInstall

              cp "$src" linuxdeploy-plugin-appimage.AppImage
              chmod +x linuxdeploy-plugin-appimage.AppImage
              ./linuxdeploy-plugin-appimage.AppImage --appimage-extract >/dev/null

              mkdir -p "$out/bin"
              cp squashfs-root/AppRun "$out/bin/linuxdeploy-plugin-appimage"
              chmod +x "$out/bin/linuxdeploy-plugin-appimage"

              runHook postInstall
            '';
          };

          appimagetool = pkgs.stdenvNoCC.mkDerivation {
            pname = "appimagetool";
            version = "continuous";

            src = pkgs.fetchurl {
              url = "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage";
              hash = "sha256-uQ9KixiWdUX9p4pEWydoChZC8e+UiM7Si2U5jyvnrdI=";
            };

            extracted = pkgs.appimageTools.extract {
              pname = "appimagetool";
              version = "continuous";
              src = pkgs.fetchurl {
                url = "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage";
                hash = "sha256-uQ9KixiWdUX9p4pEWydoChZC8e+UiM7Si2U5jyvnrdI=";
              };
            };

            dontUnpack = true;

            installPhase = ''
              runHook preInstall

              mkdir -p "$out/bin"
              cat > "$out/bin/appimagetool" <<EOF
#!${pkgs.runtimeShell}
exec "$extracted/AppRun" "\$@"
EOF
              chmod +x "$out/bin/appimagetool"

              runHook postInstall
            '';
          };

          windowsProfile = pkgs.lib.optionalString windowsCrossEnabled ''
            export WIN_CC="x86_64-w64-mingw32-gcc"
            export WIN_CXX="x86_64-w64-mingw32-g++"
            export WIN_AR="x86_64-w64-mingw32-ar"
            export WIN_RANLIB="x86_64-w64-mingw32-ranlib"
            export WIN_WINDRES="x86_64-w64-mingw32-windres"
            export WIN_STRIP="x86_64-w64-mingw32-strip"

            export WIN32_CC="i686-w64-mingw32-gcc"
            export WIN32_CXX="i686-w64-mingw32-g++"
            export WIN32_AR="i686-w64-mingw32-ar"
            export WIN32_RANLIB="i686-w64-mingw32-ranlib"
            export WIN32_WINDRES="i686-w64-mingw32-windres"
            export WIN32_STRIP="i686-w64-mingw32-strip"

            export MCFGTHREADS="${mcfgthreads}"
            export WIN32_MCFGTHREADS="${mcfgthreads32}"
            export CPATH="$MCFGTHREADS/include:$CPATH"
            export LIBRARY_PATH="$MCFGTHREADS/lib:$LIBRARY_PATH"
            export LDFLAGS="-L$MCFGTHREADS/lib $LDFLAGS"
          '';
        in
          (pkgs.buildFHSEnv {
            name = "ray-waozi-env";
            extraOutputsToInstall = [ "dev" ];

            targetPkgs = pkgs: with pkgs; [
              sdl2Pkgs.SDL2
              sdl2Pkgs.SDL2.dev
	      butler
              cmake
              curl
              curl.dev
              emscripten
              gcc
              git-lfs
              gnumake
              gradle
              imagemagick
              jdk17
              libdrm
              libdrm.dev
              libgbm
              libglvnd
              libglvnd.dev
              alsa-lib
              appimagetool
              libpulseaudio
              linuxdeploy
              linuxdeployPluginAppImage
              mesa
              ncurses
              ninja
              openssl
              openssl.dev
              pkg-config
              pipewire
              rsync
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
              export OPENSSL_INCLUDE_DIR="${pkgs.openssl.dev}/include"
              export OPENSSL_SSL_LIBRARY="${pkgs.openssl.out}/lib/libssl.so"
              export OPENSSL_CRYPTO_LIBRARY="${pkgs.openssl.out}/lib/libcrypto.so"

              # Android SDK - override any system ANDROID_HOME
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

              export RAY_RAYLIB_CONFIG="-DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0"

              export PATH="$ANDROID_HOME/cmdline-tools/11.0/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/emulator:$PATH"

              export WEB_CC="emcc"
              export WEB_AR="emar"
              export WEB_RANLIB="emranlib"

              export LINUXDEPLOY="linuxdeploy"

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

              for project in inbe; do
                if [ -d "$project/droid" ]; then
                  android_local_sdk="$ANDROID_SDK_ROOT"
                  if [ -d /mnt/storage/Android/Sdk/ndk/28.2.13676358 ]; then
                    android_local_sdk="/mnt/storage/Android/Sdk"
                  fi
                  cat > "$project/droid/local.properties" <<EOF
sdk.dir=$android_local_sdk
cmake.dir=$android_local_sdk/cmake/3.22.1
EOF
                fi
              done
            '';
          }).env;
    in {
      devShells = forAllSystems (system: {
        default = mkShell system;
      });
    };
}
