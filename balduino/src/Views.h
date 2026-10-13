#ifndef BALDUINO_VIEWS_H
#define BALDUINO_VIEWS_H

#include "Renderer.h"
#include "Types.h"

#include <string>
#include <vector>

namespace Balduino {

constexpr int HEADER_LINES = 3;
constexpr int FOOTER_LINES = 2;

std::uint8_t colorForState(char state);

void renderHeader(Renderer& r, const Snapshot& snap, const AppState& st);

void renderTable(Renderer& r, const std::vector<const ProcessInfo*>& rows, int scroll,
                 int selected, int top, int height, const AppState& st);

void renderTree(Renderer& r, const std::vector<TreeRow>& rows, int scroll, int selected,
                int top, int height, const AppState& st);

void renderThreads(Renderer& r, const ProcessInfo& proc, const std::vector<ThreadInfo>& threads,
                   int top, int height);

void renderHistory(Renderer& r, const ProcessInfo& proc, int top, int height);

void renderFooter(Renderer& r, const AppState& st);

void renderConfirm(Renderer& r, const AppState& st, const ProcessInfo& proc);

void renderNiceMenu(Renderer& r, const AppState& st, const ProcessInfo& proc);

void renderTooSmall(Renderer& r, int rows, int cols);

void putClipped(Renderer& r, int row, int col, const std::string& s, std::uint8_t fg,
                std::uint8_t bg = BLACK, std::uint8_t attr = ATTR_NONE);

}

#endif
