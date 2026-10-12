#ifndef BALDUINO_RENDERER_H
#define BALDUINO_RENDERER_H

#include <cstdint>
#include <string>
#include <vector>

namespace Balduino {

enum Color : std::uint8_t {
    BLACK = 0, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
    BRIGHT_BLACK = 8, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
    BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN, BRIGHT_WHITE,
    DEFAULT_COLOR = 9
};

enum Attr : std::uint8_t {
    ATTR_NONE      = 0,
    ATTR_BOLD      = 1,
    ATTR_DIM       = 2,
    ATTR_UNDERLINE = 4,
    ATTR_REVERSE   = 8
};

struct Cell {
    char32_t    ch   = U' ';
    std::uint8_t fg   = WHITE;
    std::uint8_t bg   = BLACK;
    std::uint8_t attr = ATTR_NONE;
};

inline bool operator==(const Cell& a, const Cell& b) {
    return a.ch == b.ch && a.fg == b.fg && a.bg == b.bg && a.attr == b.attr;
}
inline bool operator!=(const Cell& a, const Cell& b) { return !(a == b); }

class Renderer {
public:
    void resize(int rows, int cols);
    void clear();

    void put(int row, int col, char32_t ch, std::uint8_t fg, std::uint8_t bg = BLACK,
             std::uint8_t attr = ATTR_NONE);

    void putStr(int row, int col, const std::string& s, std::uint8_t fg,
                std::uint8_t bg = BLACK, std::uint8_t attr = ATTR_NONE);

    void fillRect(int r1, int c1, int r2, int c2, std::uint8_t fg, std::uint8_t bg);

    void drawBox(int r1, int c1, int r2, int c2, std::uint8_t fg, std::uint8_t bg);

    void flush();

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

private:
    void appendUtf8(std::string& out, char32_t cp);
    void emitRow(std::string& out, int row, bool withCursorMove);

    int m_rows = 0;
    int m_cols = 0;
    std::vector<Cell> m_cells;
    std::vector<Cell> m_previous;
    bool m_havePrevious = false;
};

}

#endif
