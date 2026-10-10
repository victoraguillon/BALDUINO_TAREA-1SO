#include "App.h"
#include "Terminal.h"

#include <cstdio>
#include <exception>

int main() {
    try {
        Balduino::App app;
        app.run();
    } catch (const std::exception& e) {

        Balduino::Terminal::restore();
        std::fprintf(stderr, "\nBalduino ha terminado con un error: %s\n", e.what());
        return 1;
    } catch (...) {
        Balduino::Terminal::restore();
        std::fprintf(stderr, "\nBalduino ha terminado con un error desconocido.\n");
        return 1;
    }
    return 0;
}
