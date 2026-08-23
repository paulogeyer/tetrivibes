{
  description = "Tetrivibes package and development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        tetrivibes = pkgs.stdenv.mkDerivation {
          pname = "tetrivibes";
          version = "0.1.0";
          src = pkgs.lib.cleanSource self;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            qt5.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt5.qtbase
          ];

          cmakeFlags = [
            "-DBUILD_TESTING=ON"
          ];

          doCheck = true;
          checkPhase = "ctest --output-on-failure";

          meta = {
            description = "Qt client for multiplayer tetromino combat";
            homepage = "https://github.com/paulogeyer/tetrivibes";
            license = pkgs.lib.licenses.mit;
            mainProgram = "tetrivibes";
            platforms = pkgs.lib.platforms.linux;
          };
        };
      in {
        packages = {
          default = tetrivibes;
          inherit tetrivibes;
        };

        apps.default = (flake-utils.lib.mkApp {
          drv = tetrivibes;
        }) // {
          meta = tetrivibes.meta;
        };

        checks.default = tetrivibes;

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            qt5.qtbase
            clang-tools
            gdb
          ];

          shellHook = ''
            echo "Tetrivibes development shell"
            echo "Build: cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build"
            echo "Test:  ctest --test-dir build --output-on-failure"
            echo "Run:   ./build/tetrivibes"
          '';
        };
      });
}
