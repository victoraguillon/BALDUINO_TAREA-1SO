#include "Views.h"

#include "Utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Balduino {
namespace {

const char* BRANCH    = "\u251C\u2500";
const char* LASTBR    = "\u2514\u2500";
const char* SPARKLINE = "\u2581\u2582\u2583\u2584\u2585\u2586\u2587\u2588";
constexpr int SPARK_LEVELS = 8;

struct Layout {
    int pid = 7, ppid = 7, user = 9, state = 2, cpu = 6, mem = 6;
    int rss = 7, thr = 4, pri = 3, ni = 4, time = 9, command = 20;
    int fixedTotal = 0;
    int startCommand = 0;
};

Layout computeLayout(int cols) {
    Layout w;
    w.fixedTotal = w.pid + w.ppid + w.user + w.state + w.cpu + w.mem + w.rss + w.thr +
                   w.pri + w.ni + w.time;

    int separadores = 11;
    int minComando  = 12;

    while (w.fixedTotal + separadores + minComando > cols && w.user > 4) {
        --w.user;
        --w.fixedTotal;
    }
    w.startCommand = w.fixedTotal + separadores;
    w.command      = cols - w.startCommand;
    if (w.command < 1) {

        w.command     = 0;
        w.startCommand = std::max(0, cols);
    }
    return w;
}

void putCol(Renderer& r, int row, int& col, const std::string& text, int width,
            std::uint8_t fg, std::uint8_t bg, std::uint8_t attr, bool rightAlign) {
    const std::string padded =
        rightAlign ? padLeft(text, width) : padRight(text, width);
    r.putStr(row, col, padded, fg, bg, attr);
    col += width + 1;
}

std::string treePrefix(const std::vector<TreeRow>& rows, std::size_t index) {
    const TreeRow& row = rows[index];
    std::string     prefix;

    for (int d = 0; d < row.depth - 1; ++d) {
        bool tieneMas = false;
        for (std::size_t j = index + 1; j < rows.size() && rows[j].depth > d; ++j) {
            if (rows[j].depth == d + 1) tieneMas = true;
        }
        prefix += tieneMas ? "\u2502 " : "  ";
    }
    if (row.depth > 0) prefix += row.last ? LASTBR : BRANCH;
    return prefix;
}

int modalLeft(const Renderer& r, int columnas) {
    return std::max(0, (r.cols() - columnas) / 2);
}
int modalTop(const Renderer& r, int filas) {
    return std::max(0, (r.rows() - filas) / 2);
}

void drawModal(Renderer& r, int filas, int columnas, std::uint8_t fg = WHITE,
              std::uint8_t bg = BLACK) {
    const int top    = modalTop(r, filas);
    const int left   = modalLeft(r, columnas);
    const int bottom = std::min(r.rows() - 1, top + filas - 1);
    const int right  = std::min(r.cols() - 1, left + columnas - 1);
    if (bottom <= top || right <= left) return;

    r.fillRect(top, left, bottom, right, fg, bg);
    for (int c = left; c <= right; ++c) {
        r.put(top, c, U'\u2500', fg, bg);
        r.put(bottom, c, U'\u2500', fg, bg);
    }
    r.put(top, left, U'\u250C', fg, bg);
    r.put(top, right, U'\u2510', fg, bg);
    r.put(bottom, left, U'\u2514', fg, bg);
    r.put(bottom, right, U'\u2518', fg, bg);
    for (int y = top; y <= bottom; ++y) {
        r.put(y, left, U'\u2502', fg, bg);
        r.put(y, right, U'\u2502', fg, bg);
    }
}

}

std::uint8_t colorForState(char state) {
    switch (state) {
        case 'R': return GREEN;
        case 'D': return YELLOW;
        case 'T': return CYAN;
        case 'Z': return BRIGHT_RED;
        case 'I': return BRIGHT_BLACK;
        case 'X':
        case 'x': return BRIGHT_RED;
        default:  return WHITE;
    }
}

void putClipped(Renderer& r, int row, int col, const std::string& s, std::uint8_t fg,
                std::uint8_t bg, std::uint8_t attr) {
    r.putStr(row, col, s, fg, bg, attr);
}

void renderHeader(Renderer& r, const Snapshot& snap, const AppState& st) {
    const int    cols = r.cols();
    const auto&  s    = snap.summary;

    const std::string l0 = "Procs: " + std::to_string(s.total) + "   R: " +
                           std::to_string(s.running) + "   S: " + std::to_string(s.sleeping) +
                           "   T: " + std::to_string(s.stopped) + "   Z: " +
                           std::to_string(s.zombies);
    r.putStr(0, 0, padRight(l0, cols), BLACK, BLUE, ATTR_BOLD);

    if (st.limitedMode) {
        const std::string aviso = "[modo limitado]";
        const int at = std::min(cols - static_cast<int>(aviso.size()),
                                static_cast<int>(l0.size()) + 2);
        if (at > 0) r.putStr(0, at, aviso, BLACK, YELLOW, ATTR_BOLD);
    }

    const std::string der = std::string("[") + toString(st.mode) + "]  Ord: " +
                            toString(st.sort) + (st.sortAsc ? "^" : "v") + "  Int: " +
                            formatPercent(static_cast<double>(st.intervalMs) / 1000.0, 2) + "s";
    if (static_cast<int>(der.size()) < cols) {
        r.putStr(0, cols - static_cast<int>(der.size()), der, BLACK, BLUE, ATTR_BOLD);
    }

    char  buf[128];
    std::snprintf(buf, sizeof(buf), "Load: %.2f %.2f %.2f", s.load1, s.load5, s.load15);
    std::string l1 = buf;
    l1 += "   Mem: " + formatBytes(s.memUsed) + "/" + formatBytes(s.memTotal);
    const double memPct = (s.memTotal > 0) ? static_cast<double>(s.memUsed) * 100.0 /
                                                 static_cast<double>(s.memTotal)
                                           : 0.0;
    l1 += " (" + formatPercent(memPct, 0) + "%)";
    l1 += "   Disp: " + formatBytes(s.memAvailable);
    l1 += "   CPU: " + formatPercent(s.globalCpuPercent, 0) + "%";
    l1 += "   Up: " + formatUptime(s.uptimeSeconds);
    r.putStr(1, 0, padRight(l1, cols), WHITE, BLACK);

    r.putStr(2, 0, std::string(cols, '-'), BLACK, BRIGHT_BLACK);
}

namespace {

void renderTableHeader(Renderer& r, int row, const Layout& w, const AppState& st) {
    struct H {
        const char* title;
        int         width;
        bool        right;
        SortKey     key;
    };
    const H headers[] = {
        { "PID", w.pid, true, SortKey::Pid },     { "PPID", w.ppid, true, SortKey::Pid },
        { "USER", w.user, false, SortKey::User }, { "S", w.state, false, SortKey::Cpu },
        { "%CPU", w.cpu, true, SortKey::Cpu },    { "%MEM", w.mem, true, SortKey::Mem },
        { "RSS", w.rss, true, SortKey::Mem },     { "THR", w.thr, true, SortKey::Cpu },
        { "PRI", w.pri, true, SortKey::Cpu },     { "NI", w.ni, true, SortKey::User },
        { "TIME", w.time, true, SortKey::Pid },
    };
    const char* flecha = st.sortAsc ? "^" : "v";

    bool yaMarcado = false;

    int col = 0;
    for (const H& h : headers) {
        const bool activo = (st.sort == h.key) && !yaMarcado;
        if (activo) yaMarcado = true;
        const std::string titulo =
            activo ? std::string(flecha) + h.title : std::string(h.title);
        putCol(r, row, col, titulo, h.width, activo ? BRIGHT_YELLOW : WHITE, BLACK,
               activo ? ATTR_BOLD : ATTR_NONE, h.right);
    }
    putCol(r, row, col, "COMMAND", std::max(0, r.cols() - col), WHITE, BLACK, ATTR_NONE, false);
}

}

void renderTable(Renderer& r, const std::vector<const ProcessInfo*>& rows, int scroll,
                 int selected, int top, int height, const AppState& st) {
    if (height <= 0) return;
    const int  cols = r.cols();
    const auto w    = computeLayout(cols);

    renderTableHeader(r, top, w, st);

    for (int i = 0; i < height; ++i) {
        const int index = scroll + i;
        if (index < 0 || index >= static_cast<int>(rows.size())) break;

        const ProcessInfo& p   = *rows[static_cast<std::size_t>(index)];
        const int          row = top + 1 + i;
        const bool         esSel = (index == selected);

        std::uint8_t fg = esSel ? static_cast<std::uint8_t>(BLACK) : colorForState(p.state);
        std::uint8_t bg = esSel ? static_cast<std::uint8_t>(WHITE)
                                : static_cast<std::uint8_t>(BLACK);
        if (p.noAccess) fg = esSel ? static_cast<std::uint8_t>(BLACK)
                                   : static_cast<std::uint8_t>(BRIGHT_BLACK);

        r.fillRect(row, 0, row, cols - 1, fg, bg);

        if (p.noAccess) {
            const std::string txt = "  PID " + std::to_string(p.pid) +
                                    "   [sin permiso para consultar este proceso]";
            r.putStr(row, 0, padRight(txt, cols), fg, bg);
            continue;
        }

        int col = 0;
        putCol(r, row, col, std::to_string(p.pid), w.pid, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.ppid), w.ppid, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, p.user, w.user, fg, bg, ATTR_NONE, false);
        putCol(r, row, col, std::string(1, p.state), w.state, fg, bg, ATTR_BOLD, false);

        const bool cpuAlto = (p.cpuPercent >= 50.0);
        putCol(r, row, col, formatPercent(p.cpuPercent, 1), w.cpu,
               esSel ? static_cast<std::uint8_t>(BLACK)
                     : (cpuAlto ? static_cast<std::uint8_t>(BRIGHT_GREEN) : fg),
               bg, cpuAlto ? ATTR_BOLD : ATTR_NONE, true);
        putCol(r, row, col, formatPercent(p.memPercent, 1), w.mem, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, formatBytes(p.rssBytes), w.rss, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.threads), w.thr, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.priority), w.pri, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.nice), w.ni, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, formatCpuTime(p.utime + p.stime), w.time, fg, bg, ATTR_NONE, true);

        r.putStr(row, col, padRight(p.command, std::max(0, cols - col)), fg, bg);
    }

    if (rows.empty()) {
        const std::string msg = st.filter.empty() ? "  (sin procesos que mostrar)"
                                                  : "  (ningun proceso coincide con el filtro)";
        r.putStr(top + 1, 0, padRight(msg, cols), BRIGHT_BLACK, BLACK);
    }
}

void renderTree(Renderer& r, const std::vector<TreeRow>& rows, int scroll, int selected,
                int top, int height, const AppState& st) {
    if (height <= 0) return;
    const int cols = r.cols();

    const int wPid = 7, wState = 2, wCpu = 6, wMem = 6, wRss = 7, wThr = 4, wPri = 3, wNi = 4,
              wTime = 9;
    const int otras = wPid + 1 + wState + 1 + wCpu + 1 + wMem + 1 + wRss + 1 + wThr + 1 +
                      wPri + 1 + wNi + 1 + wTime;
    const int wEtiqueta = std::max(8, cols - otras - 1);

    int col = 0;
    putCol(r, top, col, "PID", wPid, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "PROCESO (padre -> hijo)", wEtiqueta, WHITE, BLACK, ATTR_NONE, false);
    putCol(r, top, col, "S", wState, WHITE, BLACK, ATTR_NONE, false);
    putCol(r, top, col, "%CPU", wCpu, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "%MEM", wMem, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "RSS", wRss, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "THR", wThr, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "PRI", wPri, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "NI", wNi, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top, col, "TIME", wTime, WHITE, BLACK, ATTR_NONE, true);

    const int colEtiqueta = wPid + 1;
    (void)st;

    for (int i = 0; i < height; ++i) {
        const int index = scroll + i;
        if (index < 0 || index >= static_cast<int>(rows.size())) break;
        const TreeRow& tr = rows[static_cast<std::size_t>(index)];
        if (tr.proc == nullptr) continue;

        const ProcessInfo& p    = *tr.proc;
        const int          row  = top + 1 + i;
        const bool         esSel = (index == selected);
        const std::uint8_t fg = esSel ? static_cast<std::uint8_t>(BLACK) : colorForState(p.state);
        const std::uint8_t bg = esSel ? static_cast<std::uint8_t>(WHITE)
                                      : static_cast<std::uint8_t>(BLACK);

        r.fillRect(row, 0, row, cols - 1, fg, bg);

        col = 0;
        putCol(r, row, col, std::to_string(p.pid), wPid, fg, bg, ATTR_NONE, true);

        const std::string etiqueta =
            treePrefix(rows, static_cast<std::size_t>(index)) + p.shortName;
        r.putStr(row, colEtiqueta, padRight(etiqueta, wEtiqueta), fg, bg);
        col += wEtiqueta + 1;

        putCol(r, row, col, std::string(1, p.state), wState, fg, bg, ATTR_BOLD, false);
        putCol(r, row, col, formatPercent(p.cpuPercent, 1), wCpu, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, formatPercent(p.memPercent, 1), wMem, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, formatBytes(p.rssBytes), wRss, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.threads), wThr, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.priority), wPri, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(p.nice), wNi, fg, bg, ATTR_NONE, true);
        putCol(r, row, col, formatCpuTime(p.utime + p.stime), wTime, fg, bg, ATTR_NONE, true);
    }

    if (rows.empty()) {
        r.putStr(top + 1, 0, padRight("  (sin procesos que mostrar)", cols),
                 BRIGHT_BLACK, BLACK);
    }
}

void renderThreads(Renderer& r, const ProcessInfo& proc,
                   const std::vector<ThreadInfo>& threads, int top, int height) {
    const int cols = r.cols();

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Hilos de PID %d  (%s)   Hilos: %d   Nice: %d   Prioridad: %d   %%CPU: %.1f%%",
                  proc.pid, proc.shortName.c_str(), proc.threads, proc.nice, proc.priority,
                  proc.cpuPercent);
    r.putStr(top, 0, padRight(std::string(buf), cols), WHITE, BLUE, ATTR_BOLD);

    int wTid = 7, wState = 2, wPri = 4, wNi = 4, wTime = 10;
    int col = 0;
    int wNombre = std::max(10, cols - (wTid + 1 + wState + 1 + wPri + 1 + wNi + 1 + wTime + 1));

    col = 0;
    putCol(r, top + 1, col, "TID", wTid, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top + 1, col, "NAME", wNombre, WHITE, BLACK, ATTR_NONE, false);
    putCol(r, top + 1, col, "S", wState, WHITE, BLACK, ATTR_NONE, false);
    putCol(r, top + 1, col, "PRI", wPri, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top + 1, col, "NI", wNi, WHITE, BLACK, ATTR_NONE, true);
    putCol(r, top + 1, col, "TIME", wTime, WHITE, BLACK, ATTR_NONE, true);

    for (int i = 0; i < height - 2 && i < static_cast<int>(threads.size()); ++i) {
        const ThreadInfo& t = threads[static_cast<std::size_t>(i)];
        const int row = top + 2 + i;
        const std::uint8_t fg = colorForState(t.state);
        r.fillRect(row, 0, row, cols - 1, fg, BLACK);

        col = 0;
        putCol(r, row, col, std::to_string(t.tid), wTid, fg, BLACK, ATTR_NONE, true);
        putCol(r, row, col, t.name, wNombre, fg, BLACK, ATTR_NONE, false);
        putCol(r, row, col, std::string(1, t.state), wState, fg, BLACK, ATTR_BOLD, false);
        putCol(r, row, col, std::to_string(t.priority), wPri, fg, BLACK, ATTR_NONE, true);
        putCol(r, row, col, std::to_string(t.nice), wNi, fg, BLACK, ATTR_NONE, true);
        putCol(r, row, col, formatCpuTime(t.utime + t.stime), wTime, fg, BLACK, ATTR_NONE, true);
    }
}

void renderHistory(Renderer& r, const ProcessInfo& proc, int top, int height) {
    const int cols = r.cols();

    char buf[256];
    std::snprintf(buf, sizeof(buf), "Historico de CPU de PID %d  (%s)   Nice: %d   %s",
                  proc.pid, proc.shortName.c_str(), proc.nice, proc.command.c_str());
    r.putStr(top, 0, padRight(std::string(buf), cols), WHITE, BLUE, ATTR_BOLD);

    if (proc.cpuHistory.empty()) {
        r.putStr(top + 2, 0, padRight("  (todavia no hay muestras; espera un refresco)",
                                     cols), BRIGHT_BLACK, BLACK);
        return;
    }

    double maximo = 0.0, minimo = proc.cpuHistory.front(), suma = 0.0;
    for (const double v : proc.cpuHistory) {
        maximo = std::max(maximo, v);
        minimo = std::min(minimo, v);
        suma  += v;
    }
    const double promedio = suma / static_cast<double>(proc.cpuHistory.size());

    char est[256];
    std::snprintf(est, sizeof(est),
                  "  Muestras: %zu de %zu    Max: %6.2f%%    Min: %6.2f%%    Promedio: %6.2f%%",
                  proc.cpuHistory.size(), static_cast<std::size_t>(HISTORY_SIZE), maximo,
                  minimo, promedio);
    r.putStr(top + 1, 0, padRight(est, cols), WHITE, BLACK);

    const int anchoGrafico = std::max(10, cols - 4);
    const int filasGrafico = std::max(3, height - 5);

    const std::size_t n = proc.cpuHistory.size();
    std::size_t       ini = 0;
    if (n > static_cast<std::size_t>(anchoGrafico)) {
        ini = n - static_cast<std::size_t>(anchoGrafico);
    }

    r.fillRect(top + 2, 0, top + 2 + filasGrafico - 1, cols - 1, BLACK, BLACK);

    for (int c = 0; c < anchoGrafico && ini + static_cast<std::size_t>(c) < n; ++c) {
        const double v = proc.cpuHistory[ini + static_cast<std::size_t>(c)];
        const std::uint8_t fg = (v >= 50.0) ? BRIGHT_RED : (v >= 20.0 ? YELLOW : GREEN);
        const int    barras = (maximo > 0.0)
                                  ? static_cast<int>(std::lround(v / maximo * filasGrafico))
                                  : 0;

        const int nivelDibujo = (maximo > 0.0) ? std::max(barras, 1) : 1;
        const int glifoIdx = (maximo > 0.0)
                                 ? std::min(SPARK_LEVELS - 1,
                                            nivelDibujo * SPARK_LEVELS / filasGrafico)
                                 : 0;
        const std::string glifo =
            std::string(SPARKLINE).substr(static_cast<std::size_t>(glifoIdx) * 3, 3);

        for (int b = 0; b < nivelDibujo; ++b) {
            const int row = top + 2 + filasGrafico - 1 - b;
            r.putStr(row, 2 + c, glifo, fg, BLACK);
        }
    }

    if (maximo > 0.0) {
        char ref[64];
        std::snprintf(ref, sizeof(ref), "%6.1f%%", maximo);
        r.putStr(top + 2, 0, padRight(ref, std::max(0, cols - anchoGrafico - 3)), BRIGHT_BLACK,
                 BLACK, ATTR_NONE);
        r.putStr(top + 2 + filasGrafico - 1, 0, "   0.0%", BRIGHT_BLACK, BLACK, ATTR_NONE);
    }
}

void renderFooter(Renderer& r, const AppState& st) {
    const int cols = r.cols();
    const int help = std::max(0, r.rows() - FOOTER_LINES);
    const int stat = std::max(0, r.rows() - 1);

    static const char* AYUDA_COMPLETA =
        "q:Salir  +/-:Int  c:CPU  m:Mem  /:Filtro  t:Arbol  h:Hilos  g:Hist  "
        "k:Term  K:Forz  s:Susp  r:Rean  p:Prio  ?:Ayuda";
    static const char* AYUDA_MEDIA =
        "q:Salir  +/-:Int  c:CPU  m:Mem  /:Filtro  k:Term  K:Forz  ?:Ayuda";
    static const char* AYUDA_CORTA =
        "q:Salir +/-:Int c:CPU m:Mem /:Filtro ?:Ayuda";

    const char* ayuda = AYUDA_COMPLETA;
    const int   lenCorta    = static_cast<int>(std::strlen(AYUDA_CORTA));
    const int   lenMedia    = static_cast<int>(std::strlen(AYUDA_MEDIA));
    const int   lenCompleta = static_cast<int>(std::strlen(AYUDA_COMPLETA));

    std::string pos;
    if (st.rowsTotal > 0 && (st.mode == ViewMode::Table || st.mode == ViewMode::Tree)) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d-%d / %d", st.rowsFirst, st.rowsLast,
                      st.rowsTotal);
        pos = buf;
    }
    const int posLen      = static_cast<int>(pos.size());
    const bool mostrarPos = (posLen > 0) && (cols >= lenCorta + posLen + 1);
    const int  disponible = mostrarPos ? std::max(1, cols - posLen - 1) : cols;

    if (disponible < lenMedia)        ayuda = AYUDA_CORTA;
    else if (disponible < lenCompleta) ayuda = AYUDA_MEDIA;

    r.putStr(help, 0, padRight(ayuda, disponible), BRIGHT_BLACK, BLACK);
    if (mostrarPos) r.putStr(help, cols - posLen, pos, BRIGHT_CYAN, BLACK, ATTR_NONE);

    const auto  transcurrido =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - st.statusTime)
            .count();

    if (st.filtering) {
        std::string prompt = "Filtro: " + st.filterInput + "\u2588";
        if (st.filter.empty()) prompt += "   (Enter aplica, Esc cancela)";
        r.putStr(stat, 0, padRight(prompt, cols), BLACK, YELLOW, ATTR_BOLD);
    } else if (!st.statusMsg.empty() && transcurrido < STATUS_DURATION_MS) {
        const std::uint8_t fg = st.statusIsError ? BRIGHT_RED : BRIGHT_GREEN;
        r.putStr(stat, 0, padRight(" " + st.statusMsg, cols), fg, BLACK, ATTR_BOLD);
    } else if (!st.filter.empty()) {
        r.putStr(stat, 0, padRight(" Filtro activo: \"" + st.filter + "\"   (/ para cambiar)",
                                   cols), YELLOW, BLACK, ATTR_NONE);
    } else {
        r.putStr(stat, 0, std::string(cols, ' '), WHITE, BLACK);
    }
}

void renderConfirm(Renderer& r, const AppState& st, const ProcessInfo& proc) {
    const bool  forzar   = (st.confirmKind == ConfirmKind::ForceKill);
    const int   columnas = std::min(r.cols() - 4, forzar ? 58 : 50);
    const int   filas    = 8;
    const int   top      = modalTop(r, filas);
    const int   left     = modalLeft(r, columnas);

    drawModal(r, filas, columnas);

    std::string titulo;
    switch (st.confirmKind) {
        case ConfirmKind::Terminate:  titulo = "Confirmar: TERMINAR"; break;
        case ConfirmKind::ForceKill:  titulo = "Confirmar: TERMINAR POR LA FUERZA"; break;
        case ConfirmKind::Suspend:    titulo = "Confirmar: SUSPENDER"; break;
        case ConfirmKind::Resume:     titulo = "Confirmar: REANUDAR"; break;
        default:                      titulo = "Confirmar"; break;
    }
    r.putStr(top + 1, left + 2, padRight(titulo, columnas - 4), BRIGHT_YELLOW, BLACK,
             ATTR_BOLD);

    char buf[512];
    std::snprintf(buf, sizeof(buf), "Proceso: %d  (%s)", proc.pid, proc.shortName.c_str());
    r.putStr(top + 2, left + 2, padRight(buf, columnas - 4), WHITE, BLACK);

    std::string cmd = proc.command;
    if (static_cast<int>(cmd.size()) > columnas - 6) cmd = cmd.substr(0, columnas - 9) + "...";
    r.putStr(top + 3, left + 2, padRight("  " + cmd, columnas - 4), BRIGHT_BLACK, BLACK);

    if (forzar && st.confirmPhase == ConfirmPhase::Prompt) {
        r.putStr(top + 4, left + 2,
                 padRight("SIGKILL no se puede deshacer.", columnas - 4), BRIGHT_RED, BLACK);
        r.putStr(top + 5, left + 2,
                 padRight("Esta accion requiere doble confirmacion.", columnas - 4),
                 WHITE, BLACK);
        r.putStr(top + 6, left + 2, padRight("Continuar?  [s/N]", columnas - 4), BRIGHT_GREEN,
                 BLACK, ATTR_BOLD);
    } else if (forzar) {

        r.putStr(top + 4, left + 2,
                 padRight("Para confirmar escribe: " + std::string(FORCE_WORD), columnas - 4),
                 BRIGHT_RED, BLACK, ATTR_BOLD);
        std::string prompt = "  " + st.confirmInput + "\u2588";
        r.putStr(top + 5, left + 2, padRight(prompt, columnas - 4), WHITE, BLACK);
        r.putStr(top + 6, left + 2, padRight("(Enter confirma, Esc cancela)", columnas - 4),
                 BRIGHT_BLACK, BLACK);
    } else {
        r.putStr(top + 5, left + 2, padRight("Continuar?  [s/N]", columnas - 4), BRIGHT_GREEN,
                 BLACK, ATTR_BOLD);
    }
}

void renderNiceMenu(Renderer& r, const AppState& st, const ProcessInfo& proc) {
    const int filas    = NICE_CHOICE_COUNT + 6;

    const int columnas = std::min(r.cols() - 4, 56);
    const int ancho    = std::max(1, columnas - 4);
    const int top      = modalTop(r, filas);
    const int left     = modalLeft(r, columnas);

    drawModal(r, filas, columnas);

    char buf[256];
    std::snprintf(buf, sizeof(buf), "Prioridad (nice) de PID %d (%s)", proc.pid,
                  proc.shortName.c_str());
    r.putStr(top + 1, left + 2, padRight(buf, ancho), BRIGHT_YELLOW, BLACK, ATTR_BOLD);

    std::snprintf(buf, sizeof(buf), "  nice actual: %d", proc.nice);
    r.putStr(top + 2, left + 2, padRight(buf, ancho), WHITE, BLACK);

    if (!st.limitedMode) {
        r.putStr(top + 3, left + 2,
                 padRight("  (root: puedes usar valores negativos)", ancho),
                 BRIGHT_GREEN, BLACK);
    } else {
        r.putStr(top + 3, left + 2,
                 padRight("  (modo limitado: los negativos requieren root)", ancho),
                 YELLOW, BLACK);
    }

    for (int i = 0; i < NICE_CHOICE_COUNT; ++i) {
        const int   fila  = top + 4 + i;
        const bool  sel   = (i == st.niceIndex);
        const char* marca = (NICE_CHOICES[i] == proc.nice) ? " (actual)" : "";
        std::snprintf(buf, sizeof(buf), "%s %+3d%s", sel ? ">" : " ", NICE_CHOICES[i], marca);
        r.putStr(fila, left + 2, padRight(buf, ancho),
                 sel ? BLACK : WHITE, sel ? YELLOW : BLACK, sel ? ATTR_BOLD : ATTR_NONE);
    }

    r.putStr(top + 4 + NICE_CHOICE_COUNT, left + 2,
             padRight("  Enter aplica   Esc cancela", ancho), BRIGHT_BLACK, BLACK);
}

void renderTooSmall(Renderer& r, int rows, int cols) {
    r.clear();
    const std::string l1 =
        "Terminal demasiado pequena: " + std::to_string(cols) + " columnas x " +
        std::to_string(rows) + " filas";
    const std::string l2 = "Balduino necesita al menos " + std::to_string(MIN_COLS) +
                           " columnas x " + std::to_string(MIN_ROWS) + " filas";
    const std::string l3 = "Amplia la ventana para ver la tabla.";

    const int y = std::max(0, (rows - 3) / 2);
    r.putStr(y, 0, padRight(l1, cols), BRIGHT_RED, BLACK, ATTR_BOLD);
    r.putStr(y + 1, 0, padRight(l2, cols), YELLOW, BLACK);
    r.putStr(y + 2, 0, padRight(l3, cols), WHITE, BLACK);
}

}
