#include "Renderer.h"

#include <unistd.h>

#include <algorithm>
#include <cstdio>

namespace Balduino {
namespace {

void appendInt(std::string& out, int value) {
    char buf[16];
    const int n = std::snprintf(buf, sizeof(buf), "%d", value);
    out.append(buf, static_cast<std::size_t>(n > 0 ? n : 0));
}

}

void Renderer::resize(int rows, int cols) {
    if (rows < 0) rows = 0;
    if (cols < 0) cols = 0;
    m_rows = rows;
    m_cols = cols;
    m_cells.assign(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), Cell{});
    m_previous.clear();
    m_havePrevious = false;
    clear();
}

void Renderer::clear() {
    std::fill(m_cells.begin(), m_cells.end(), Cell{});
}

void Renderer::put(int row, int col, char32_t ch, std::uint8_t fg, std::uint8_t bg,
                   std::uint8_t attr) {
    if (row < 0 || col < 0 || row >= m_rows || col >= m_cols) return;
    const std::size_t index = static_cast<std::size_t>(row) * static_cast<std::size_t>(m_cols) +
                              static_cast<std::size_t>(col);
    Cell& cell = m_cells[index];
    cell.ch   = ch;
    cell.fg   = fg;
    cell.bg   = bg;
    cell.attr = attr;
}

void Renderer::putStr(int row, int col, const std::string& s, std::uint8_t fg,
                      std::uint8_t bg, std::uint8_t attr) {
    int c = col;
    for (std::size_t i = 0; i < s.size();) {
        if (c >= m_cols) break;

        const unsigned char lead = static_cast<unsigned char>(s[i]);
        char32_t cp   = 0;
        int      size = 0;
        if (lead < 0x80)             { cp = lead;         size = 1; }
        else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1Fu; size = 2; }
        else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0Fu; size = 3; }
        else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07u; size = 4; }
        else { ++i; continue; }

        if (i + static_cast<std::size_t>(size) > s.size()) break;
        for (int k = 1; k < size; ++k) {
            cp = (cp << 6) |
                 (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3Fu);
        }
        put(row, c, cp, fg, bg, attr);
        ++c;
        i += static_cast<std::size_t>(size);
    }
}

void Renderer::fillRect(int r1, int c1, int r2, int c2, std::uint8_t fg, std::uint8_t bg) {
    const int rowStart = std::max(0, r1);
    const int rowEnd   = std::min(m_rows - 1, r2);
    for (int r = rowStart; r <= rowEnd; ++r) {
        for (int c = std::max(0, c1); c <= std::min(m_cols - 1, c2); ++c) put(r, c, U' ', fg, bg);
    }
}

void Renderer::drawBox(int r1, int c1, int r2, int c2, std::uint8_t fg, std::uint8_t bg) {
    if (r2 <= r1 || c2 <= c1) return;
    if (r1 < 0 || c1 < 0 || r1 >= m_rows || c1 >= m_cols) return;

    const int bottom = std::min(r2, m_rows - 1);
    const int right  = std::min(c2, m_cols - 1);

    for (int c = c1; c <= right; ++c) {
        put(r1, c, (c == c1) ? U'\u250C' : (c == right ? U'\u2510' : U'\u2500'), fg, bg);
        put(bottom, c, (c == c1) ? U'\u2514' : (c == right ? U'\u2518' : U'\u2500'), fg, bg);
    }

    for (int r = r1; r <= bottom; ++r) {
        put(r, c1, U'\u2502', fg, bg);
        put(r, right, U'\u2502', fg, bg);
    }
}

void Renderer::appendUtf8(std::string& out, char32_t cp) {
    const auto v = static_cast<std::uint32_t>(cp);
    if (v < 0x80) {
        out.push_back(static_cast<char>(v));
    } else if (v < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (v >> 6)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3F)));
    } else if (v < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (v >> 12)));
        out.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (v >> 18)));
        out.push_back(static_cast<char>(0x80 | ((v >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3F)));
    }
}

void Renderer::emitRow(std::string& out, int row, bool withCursorMove) {
    if (withCursorMove) {
        out += "\x1b[";
        appendInt(out, row + 1);
        out += ";1H";
    }

    int            lastFg   = -1;
    int            lastBg   = -1;
    int            lastAttr = -1;
    const std::size_t base = static_cast<std::size_t>(row) * static_cast<std::size_t>(m_cols);

    for (int c = 0; c < m_cols; ++c) {
        const Cell& cell = m_cells[base + static_cast<std::size_t>(c)];

        if (static_cast<int>(cell.fg) != lastFg || static_cast<int>(cell.bg) != lastBg ||
            static_cast<int>(cell.attr) != lastAttr) {
            out += "\x1b[0";
            if (cell.attr & ATTR_BOLD)      out += ";1";
            if (cell.attr & ATTR_DIM)       out += ";2";
            if (cell.attr & ATTR_UNDERLINE) out += ";4";
            if (cell.attr & ATTR_REVERSE)   out += ";7";
            out += ";3";
            appendInt(out, cell.fg);
            out += ";4";
            appendInt(out, cell.bg);
            out += 'm';
            lastFg   = cell.fg;
            lastBg   = cell.bg;
            lastAttr = cell.attr;
        }
        appendUtf8(out, cell.ch);
    }
    out += "\x1b[0m";
}

void Renderer::flush() {
    if (m_rows <= 0 || m_cols <= 0) return;

    std::string out;
    out.reserve(static_cast<std::size_t>(m_rows) * static_cast<std::size_t>(m_cols) * 4);

    if (m_havePrevious && m_previous.size() == m_cells.size()) {

        for (int r = 0; r < m_rows; ++r) {
            const std::size_t base = static_cast<std::size_t>(r) * static_cast<std::size_t>(m_cols);
            const bool changed = !std::equal(m_cells.begin() + static_cast<std::ptrdiff_t>(base),
                                             m_cells.begin() + static_cast<std::ptrdiff_t>(base + static_cast<std::size_t>(m_cols)),
                                             m_previous.begin() + static_cast<std::ptrdiff_t>(base));
            if (changed) emitRow(out, r, true);
        }
    } else {
        for (int r = 0; r < m_rows; ++r) emitRow(out, r, true);
    }

    if (out.empty()) {
        m_previous = m_cells;
        m_havePrevious = true;
        return;
    }

    std::size_t written = 0;
    while (written < out.size()) {
        const ssize_t n = ::write(STDOUT_FILENO, out.data() + written, out.size() - written);
        if (n <= 0) break;
        written += static_cast<std::size_t>(n);
    }

    m_previous      = m_cells;
    m_havePrevious  = true;
}

}
