{ stdenv, autoreconfHook, pkg-config, html-tidy, zimlib, }:
stdenv.mkDerivation {
  pname = "zim-text-extractor";
  version = "1.0.0-git";
  src = ./.;
  nativeBuildInputs = [ autoreconfHook pkg-config ];
  buildInputs = [ html-tidy zimlib ];
}
