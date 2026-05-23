{
  description = "Raylib desktop and Android development environment";

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

      rayShell = pkgs.buildFHSEnv {
        name = "ray-inbe-env";

        targetPkgs = pkgs: with pkgs; [
          SDL2
          cmake
          gcc
          gnumake
          gradle
          jdk17
          keytool
          libdrm
          libgbm
          libglvnd
          mesa
          ninja
          pkg-config
          zlib
        ];

        profile = ''
          export JAVA_HOME="${pkgs.jdk17.home}"
          export ANDROID_HOME="${sdk}/libexec/android-sdk"
          export ANDROID_SDK_ROOT="$ANDROID_HOME"
          export ANDROID_NDK_ROOT="${ndkPath}"
          export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

          if [ ! -f android/local.properties ]; then
            cat > android/local.properties <<EOF
sdk.dir=$ANDROID_SDK_ROOT
ndk.dir=$ANDROID_NDK_ROOT
cmake.dir=$ANDROID_HOME/cmake/3.22.1
EOF
          fi
        '';
      };
    in {
      devShells.${system}.default = rayShell.env;
    };
}
