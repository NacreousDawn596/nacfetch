{
  description = "Nacfetch - fast C++ system information fetcher";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
  let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    packages.${system}.default = pkgs.stdenv.mkDerivation {
      pname = "nacfetch";
      version = "0.0.1";

      src = self;

      nativeBuildInputs = [
        pkgs.cmake
      ];

      buildPhase = ''
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build
      '';

      installPhase = ''
        mkdir -p $out/bin
        cp output/nacfetch-linux-generic $out/bin/nacfetch
      '';

      meta = with pkgs.lib; {
        description = "Fast modern C++ system information fetcher";
        homepage = "https://github.com/YOUR_USERNAME/nacfetch";
        license = licenses.gpl3;
        platforms = platforms.linux;
      };
    };

    apps.${system}.default = {
      type = "app";
      program = "${self.packages.${system}.default}/bin/nacfetch";
    };
  };
}
