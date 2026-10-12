#ifndef BALDUINO_TERMINAL_H
#define BALDUINO_TERMINAL_H

namespace Balduino {

class Terminal {
public:

    static void init();

    static void restore();

    static void restoreNow();

    static void getSize(int& rows, int& cols);

    static bool hasResized();

    static bool shouldQuit();
};

}

#endif
