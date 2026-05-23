{
  description = "Minimalist Bare-Metal Android NDK Development Environment";

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

    in {
      devShells.${system}.default = (pkgs.buildFHSEnv {
        name = "android-fhs-env";

        targetPkgs = pkgs: with pkgs; [
          gnumake
          cmake
          ninja
          gradle
          jdk17
          glibc
          zlib
          ncurses
        ];

        profile = ''
          export JAVA_HOME="${pkgs.jdk17.home}"
          export ANDROID_HOME="${sdk}/libexec/android-sdk"
          export ANDROID_SDK_ROOT="${sdk}/libexec/android-sdk"
          export ANDROID_NDK_ROOT="${ndkPath}"
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