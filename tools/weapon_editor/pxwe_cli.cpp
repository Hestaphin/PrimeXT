// Portable validation entry point. Windows builds use pxwe_win32.cpp for the GUI.
#include "pxwe_project.h"
#include <iostream>
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "pxwe J.10.6.9.9 (portable validation interface)\n"; return 0;
    }
    if (argc != 3 || std::string(argv[1]) != "--validate") {
        std::cerr << "usage: pxwe --validate <game-folder>\n"
            "The native graphical weapon editor is available on Windows.\n"; return 2;
    }
    try {
        pxwe::Project project; project.Load(pxwe::fs::u8path(argv[2]));
        const auto validation = project.Validate();
        (validation.Ok() ? std::cout : std::cerr) << validation.Message() << '\n';
        return validation.Ok() ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
