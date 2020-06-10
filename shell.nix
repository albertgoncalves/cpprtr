with import <nixpkgs> {};
mkShell {
    buildInputs = [
        clang_10
        cppcheck
        gdb
        glibcLocales
        linuxPackages.perf
        python3
        shellcheck
        valgrind
    ];
    shellHook = ''
        . .shellhook
    '';
}
