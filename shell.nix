with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_11.stdenv; } {
    buildInputs = [
        feh
        linuxPackages.perf
        python3
        shellcheck
        valgrind
    ];
    shellHook = ''
        . .shellhook
    '';
}
