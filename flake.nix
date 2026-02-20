{
  description = "Kisak-Strike";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      inherit (nixpkgs) lib;

      eachSystem = lib.genAttrs lib.systems.flakeExposed (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          llvm = pkgs.llvmPackages_22;

          runtimePath = lib.makeLibraryPath [
            pkgs.vulkan-loader
            pkgs.mesa
          ];

          buildInputs = with pkgs; [
            SDL2
            freetype
            fontconfig
            libx11
            vulkan-loader
            (zlib-ng.override { withZlibCompat = true; })
            libjpeg
            libpng
          ];

          nativeBuildInputs = with pkgs; [
            python3
            pkg-config
            mold
          ];

          ks = llvm.stdenv.mkDerivation {
            pname = "ks";
            version = "0.1.0";
            src = ./src;

            nativeBuildInputs = nativeBuildInputs ++ [ pkgs.wafHook ];
            inherit buildInputs;

            env.MARCH = "x86-64-v3";
          };
        in
        {
          packages.default = ks;

          apps = {
            default = {
              type = "app";
              program = toString (
                pkgs.writeShellScript "ks" ''
                  set -euo pipefail
                  dir="''${1:-game}"
                  mkdir -p "$dir/bin" "$dir/csgo/bin"
                  install -m755 ${ks}/srceng "$dir/"
                  install -m755 -t "$dir/bin/" ${ks}/bin/*
                  install -m755 -t "$dir/csgo/bin/" ${ks}/csgo/bin/*
                  export LD_LIBRARY_PATH="${runtimePath}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
                  export XDG_DATA_DIRS="${pkgs.mesa}/share''${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
                  cd "$dir"
                  exec ./srceng "''${@:2}"
                ''
              );
            };

            setup = {
              type = "app";
              program = toString (
                pkgs.writeShellScript "ks-setup" ''
                  set -euo pipefail
                  dir="''${1:-game}"
                  mkdir -p "$dir"
                  ${pkgs.depotdownloader}/bin/DepotDownloader \
                    -app 730 -depot 731 \
                    -manifest 7043469183016184477 \
                    -max-downloads 16 \
                    -validate -dir "$dir"
                  echo 730 > "$dir/steam_appid.txt"
                ''
              );
            };
          };

          devShells.default = llvm.stdenv.mkDerivation {
            name = "ks-dev";
            NIX_ENFORCE_NO_NATIVE = false;
            NIX_HARDENING_DISABLE = true;
            nativeBuildInputs = nativeBuildInputs ++ [
              pkgs.waf
              pkgs.ccache
              llvm.clang-tools
            ];
            inherit buildInputs;

            shellHook = ''
              export LD_LIBRARY_PATH="${runtimePath}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              export XDG_DATA_DIRS="${pkgs.mesa}/share''${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
            '';
          };
        }
      );
    in
    {
      packages = lib.mapAttrs (_: v: v.packages) eachSystem;
      devShells = lib.mapAttrs (_: v: v.devShells) eachSystem;
      apps = lib.mapAttrs (_: v: v.apps) eachSystem;
    };
}
