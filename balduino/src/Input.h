#ifndef BALDUINO_INPUT_H
#define BALDUINO_INPUT_H

namespace Balduino {

enum class KeyType {
    None, Char, Up, Down, Left, Right,
    PageUp, PageDown, Home, End,
    Enter, Escape, Backspace, Delete, Tab, Resize
};

struct Key {
    KeyType type = KeyType::None;
    char    ch   = '\0';

    bool isChar() const { return type == KeyType::Char; }
    bool isNone() const { return type == KeyType::None; }
};

class Input {
public:

    static Key readKey(int timeoutMs);

    static void drain();
};

}

#endif
