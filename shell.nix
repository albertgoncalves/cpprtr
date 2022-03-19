with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_14.stdenv; } {
    buildInputs = [
        feh
        linuxPackages.perf
        llvmPackages_12.lld
        python3
        shellcheck
        valgrind
    ];
    shellHook = ''
        . .shellhook
    '';
}
