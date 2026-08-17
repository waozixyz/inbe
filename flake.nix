{
  description = "Inner Breeze breathing, meditation, and habit practice app";

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
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          versionLine =
            pkgs.lib.lists.findFirst
              (line: builtins.match ''#define INBE_VERSION_STRING "([^"]+)".*'' line != null)
              null
              (pkgs.lib.strings.splitString "\n" (builtins.readFile ./src/core/version.h));
          versionMatch = builtins.match ''#define INBE_VERSION_STRING "([^"]+)".*'' versionLine;
        in {
          default = pkgs.stdenv.mkDerivation rec {
            pname = "inbe";
            version = builtins.elemAt versionMatch 0;

            src = self;

            nativeBuildInputs = with pkgs; [
              cmake
              gnumake
              pkg-config
            ];

            buildInputs = with pkgs; [
              curl
              gtk3
              libdrm
              libglvnd
              mesa
              openssl
              SDL2
              zlib
            ];

            buildPhase = ''
              runHook preBuild
              make native
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              install -d "$out/bin"
              install -m755 "$(find build/bin/linux -maxdepth 1 -type f -name 'inbe-linux-*' | head -n 1)" "$out/bin/inbe"

              install -D -m644 packaging/linux/appimage/inbe.desktop \
                "$out/share/applications/inbe.desktop"
              install -D -m644 packaging/linux/appimage/inbe.png \
                "$out/share/icons/hicolor/512x512/apps/inbe.png"
              install -D -m644 packaging/linux/appimage/inbe.appdata.xml \
                "$out/share/metainfo/xyz.waozi.inbe.metainfo.xml"

              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "Syncable breathing, meditation, and habit practice app";
              homepage = "https://inbe.waozi.xyz/";
              license = licenses.bsd3;
              mainProgram = "inbe";
              platforms = platforms.linux;
            };
          };
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/inbe";
        };
      });
    };
}
