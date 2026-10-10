#include "App.h"
#include "ProcActions.h"
#include "ProcReader.h"
#include "ProcStats.h"
#include "Terminal.h"
#include "Utils.h"
#include "Views.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Balduino {
namespace {

using Clock = std::chrono::steady_clock;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
        .count();
}

bool matchesFilter(const ProcessInfo& p, const std::string& filter) {
    if (filter.empty()) return true;
    if (toLower(p.shortName).find(filter) != std::string::npos) return true;
    if (toLower(p.command).find(filter) != std::string::npos) return true;
    return false;
}

}

int App::bodyTop() const {
    return HEADER_LINES;
}

int App::bodyHeight() const {
    const int total = m_renderer.rows() - HEADER_LINES - FOOTER_LINES;
    return total > 0 ? total : 0;
}

void App::refresh() {
    Snapshot nueva = readSnapshot();

    if (m_haveSnapshot) {
        computeCpu(nueva, m_snapshot);
    }
    computeMem(nueva);
    classifyProcesses(nueva);

    m_snapshot = nueva;
    m_haveSnapshot = true;

    rebuildVisible();
    rebuildTree();
    resolveSelection();

    if (m_state.mode == ViewMode::Threads && m_state.targetPid > 0) {
        if (processByPid(m_state.targetPid) != nullptr) {
            m_threads = threadsOf(m_state.targetPid);
        }
    }
}

const ProcessInfo* App::processByPid(int pid) const {
    for (const ProcessInfo& p : m_snapshot.processes) {
        if (p.pid == pid) return &p;
    }
    return nullptr;
}

const ProcessInfo* App::selectedProcess() const {
    if (m_state.selected < 0 ||
        m_state.selected >= static_cast<int>(m_visible.size())) {
        return nullptr;
    }
    return m_visible[static_cast<std::size_t>(m_state.selected)];
}

std::vector<ThreadInfo> App::threadsOf(int pid) const {
    return readThreads(pid);
}

void App::rebuildVisible() {
    m_visible.clear();
    const std::string filtro = toLower(m_state.filter);

    for (const ProcessInfo& p : m_snapshot.processes) {
        if (matchesFilter(p, filtro)) m_visible.push_back(&p);
    }

    const SortKey key   = m_state.sort;
    const bool    asc   = m_state.sortAsc;

    std::stable_sort(m_visible.begin(), m_visible.end(),
                     [key, asc](const ProcessInfo* a, const ProcessInfo* b) {
                         int cmp = 0;
                         switch (key) {
                             case SortKey::Cpu:
                                 cmp = (a->cpuPercent < b->cpuPercent)   ? -1
                                       : (a->cpuPercent > b->cpuPercent) ? 1 : 0;
                                 break;
                             case SortKey::Mem:
                                 cmp = (a->rssBytes < b->rssBytes) ? -1
                                       : (a->rssBytes > b->rssBytes) ? 1 : 0;
                                 break;
                             case SortKey::Pid:
                                 cmp = (a->pid < b->pid) ? -1 : (a->pid > b->pid) ? 1 : 0;
                                 break;
                             case SortKey::User:
                                 cmp = a->user.compare(b->user);
                                 if (cmp == 0) cmp = (a->pid < b->pid) ? -1 : 1;
                                 break;
                             default: cmp = 0; break;
                         }
                         if (cmp == 0) return a->pid < b->pid;
                         return asc ? cmp < 0 : cmp > 0;
                     });
}

void App::rebuildTree() {
    m_treeRows.clear();
    m_treeProcs.clear();
    if (m_visible.empty()) return;

    std::unordered_map<int, const ProcessInfo*> byPid;
    std::unordered_map<int, std::vector<int>>  children;
    byPid.reserve(m_visible.size());

    for (const ProcessInfo* p : m_visible) {
        byPid.emplace(p->pid, p);
        children[p->ppid].push_back(p->pid);
    }

    std::unordered_set<int> visitados;

    std::function<void(int, int, bool)> añadir =
        [&](int pid, int depth, bool ultimo) {
            if (visitados.count(pid) != 0) return;
            visitados.insert(pid);
            const auto it = byPid.find(pid);
            if (it == byPid.end()) return;

            TreeRow fila;
            fila.proc  = it->second;
            fila.depth = depth;
            fila.last  = ultimo;
            m_treeRows.push_back(fila);
            m_treeProcs.push_back(it->second);

            const auto itHijos = children.find(pid);
            if (itHijos == children.end() || depth >= 64) return;

            const std::vector<int>& kids = itHijos->second;
            for (std::size_t i = 0; i < kids.size(); ++i) {
                añadir(kids[i], depth + 1, i + 1 == kids.size());
            }
        };

    for (const ProcessInfo* p : m_visible) {
        if (p->ppid == 0 || byPid.find(p->ppid) == byPid.end()) {
            añadir(p->pid, 0, true);
        }
    }

    for (const ProcessInfo* p : m_visible) {
        if (visitados.count(p->pid) == 0) {
            añadir(p->pid, 0, true);
        }
    }
}

const std::vector<const ProcessInfo*>& App::activeRows() const {
    return (m_state.mode == ViewMode::Tree) ? m_treeProcs : m_visible;
}

void App::clampScroll() {
    const std::vector<const ProcessInfo*>& rows = activeRows();
    const int n = static_cast<int>(rows.size());

    if (n == 0) {
        m_state.selected    = 0;
        m_state.scroll      = 0;
        m_state.selectedPid = -1;
        return;
    }

    m_state.selected = std::min(std::max(0, m_state.selected), n - 1);

    m_state.selectedPid = rows[static_cast<std::size_t>(m_state.selected)]->pid;

    const int utiles = std::max(1, bodyHeight() - 1);
    if (m_state.selected < m_state.scroll) {
        m_state.scroll = m_state.selected;
    } else if (m_state.selected >= m_state.scroll + utiles) {
        m_state.scroll = m_state.selected - utiles + 1;
    }
    m_state.scroll = std::max(0, std::min(m_state.scroll, std::max(0, n - utiles)));
}

void App::resolveSelection() {
    const std::vector<const ProcessInfo*>& rows = activeRows();
    const int n = static_cast<int>(rows.size());

    if (n == 0) {
        m_state.selected    = 0;
        m_state.scroll      = 0;
        m_state.selectedPid = -1;
        return;
    }

    int nuevo = -1;
    if (m_state.selectedPid > 0) {
        for (int i = 0; i < n; ++i) {
            if (rows[static_cast<std::size_t>(i)]->pid == m_state.selectedPid) {
                nuevo = i;
                break;
            }
        }
    }
    if (nuevo < 0) {
        nuevo = std::min(std::max(0, m_state.selected), n - 1);
    }
    m_state.selected = nuevo;

    clampScroll();
}

void App::setMode(ViewMode mode) {
    m_state.mode = mode;

    resolveSelection();
}

void App::navigate(int delta) {

    if (m_state.mode == ViewMode::Threads || m_state.mode == ViewMode::History) return;

    const int n = static_cast<int>(activeRows().size());
    if (n == 0) return;

    m_state.selected = std::min(std::max(0, m_state.selected + delta), n - 1);
    clampScroll();
}

void App::treeNavParent() {
    if (m_state.mode != ViewMode::Tree) return;
    const int idx = m_state.selected;
    if (idx <= 0 || idx >= static_cast<int>(m_treeRows.size())) return;

    const int depth = m_treeRows[static_cast<std::size_t>(idx)].depth;
    for (int i = idx - 1; i >= 0; --i) {
        if (m_treeRows[static_cast<std::size_t>(i)].depth < depth) {
            m_state.selected = i;
            clampScroll();
            return;
        }
    }
}

void App::treeNavChild() {
    if (m_state.mode != ViewMode::Tree) return;
    const int idx = m_state.selected;
    if (idx < 0 || idx + 1 >= static_cast<int>(m_treeRows.size())) return;

    if (m_treeRows[static_cast<std::size_t>(idx) + 1].depth >
        m_treeRows[static_cast<std::size_t>(idx)].depth) {
        m_state.selected = idx + 1;
        clampScroll();
    }
}

void App::adjustInterval(int deltaMs) {
    const int antes = m_state.intervalMs;
    m_state.intervalMs = std::max(MIN_INTERVAL_MS,
                                  std::min(MAX_INTERVAL_MS, m_state.intervalMs + deltaMs));
    if (m_state.intervalMs == antes) {
        setStatus(deltaMs > 0 ? "Intervalo maximo alcanzado (10.00 s)"
                              : "Intervalo minimo alcanzado (0.25 s)");
    } else {
        setStatus("Intervalo de refresco: " +
                  formatPercent(static_cast<double>(m_state.intervalMs) / 1000.0, 2) + " s");
    }
}

void App::sortBy(SortKey key) {
    if (m_state.sort == key) {
        m_state.sortAsc = !m_state.sortAsc;
    } else {
        m_state.sort    = key;
        m_state.sortAsc = false;
    }
    rebuildVisible();
    rebuildTree();
    resolveSelection();
    setStatus(std::string("Ordenado por ") + toString(m_state.sort) +
              (m_state.sortAsc ? " (ascendente)" : " (descendente)"));
}

void App::setStatus(const std::string& msg, bool isError) {
    m_state.statusMsg     = msg;
    m_state.statusIsError = isError;
    m_state.statusTime    = Clock::now();
}

void App::openConfirm(ConfirmKind kind) {
    const ProcessInfo* p = selectedProcess();
    if (p == nullptr) {
        setStatus("No hay ningun proceso seleccionado", true);
        return;
    }
    if (p->noAccess) {
        setStatus("No hay permiso para consultar el proceso " + std::to_string(p->pid), true);
        return;
    }

    if (p->isZombie && (kind == ConfirmKind::Terminate || kind == ConfirmKind::ForceKill)) {
        setStatus("El proceso " + std::to_string(p->pid) +
                      " ya termino y es un zombie; solo su padre puede recogerlo",
                  true);
        return;
    }

    m_state.showConfirm   = true;
    m_state.confirmKind   = kind;
    m_state.confirmPhase  = ConfirmPhase::Prompt;
    m_state.confirmInput.clear();
    m_state.confirmPid    = p->pid;
    m_state.confirmName   = p->shortName;
    Input::drain();
}

void App::executeConfirmedAction() {
    const int         pid  = m_state.confirmPid;
    const ConfirmKind kind = m_state.confirmKind;
    const std::string nom  = m_state.confirmName;

    m_state.showConfirm = false;
    m_state.confirmKind = ConfirmKind::None;
    m_state.confirmInput.clear();

    if (pid <= 0) {
        setStatus("Accion cancelada: no hay proceso seleccionado", true);
        return;
    }

    std::string msg;
    bool ok = false;

    switch (kind) {
        case ConfirmKind::Terminate:
            ok = terminate(pid, msg);
            break;
        case ConfirmKind::ForceKill:
            ok = forceTerminate(pid, msg);
            break;
        case ConfirmKind::Suspend:
            ok = suspend(pid, msg);
            break;
        case ConfirmKind::Resume:
            ok = resume(pid, msg);
            break;
        default:
            msg = "Accion desconocida";
            break;
    }

    if (ok) {
        const std::string accion = (kind == ConfirmKind::Terminate) ? "terminado"
                                   : (kind == ConfirmKind::ForceKill) ? "forzado"
                                   : (kind == ConfirmKind::Suspend)   ? "suspendido"
                                                                      : "reanudado";
        setStatus("Proceso " + std::to_string(pid) + " (" + nom + ") " + accion);

        m_nextRefreshMs = 0;
    } else {
        setStatus(msg, true);
    }
}

void App::openNiceMenu() {
    const ProcessInfo* p = selectedProcess();
    if (p == nullptr) {
        setStatus("No hay ningun proceso seleccionado", true);
        return;
    }
    m_state.showNiceMenu  = true;
    m_state.niceTargetPid = p->pid;

    for (int i = 0; i < NICE_CHOICE_COUNT; ++i) {
        if (NICE_CHOICES[i] >= p->nice) {
            m_state.niceIndex = i;
            break;
        }
        m_state.niceIndex = i;
    }
    Input::drain();
}

void App::toggleHelp() {
    m_showHelp = !m_showHelp;
    Input::drain();
}

void App::handleConfirmKey(const Key& k) {

    if (m_state.confirmKind == ConfirmKind::ForceKill &&
        m_state.confirmPhase == ConfirmPhase::ConfirmWord) {
        if (k.type == KeyType::Escape) {
            m_state.showConfirm = false;
            m_state.confirmKind = ConfirmKind::None;
            m_state.confirmInput.clear();
            setStatus("Cancelado: no se ha terminado el proceso");
            return;
        }
        if (k.type == KeyType::Backspace) {
            if (!m_state.confirmInput.empty()) m_state.confirmInput.pop_back();
            return;
        }
        if (k.type == KeyType::Enter) {
            if (m_state.confirmInput == FORCE_WORD) {
                executeConfirmedAction();
            } else {
                m_state.showConfirm = false;
                m_state.confirmKind = ConfirmKind::None;
                m_state.confirmInput.clear();
                setStatus("Cancelado: la palabra no coincide, el proceso sigue vivo", true);
            }
            return;
        }
        if (k.isChar() && k.ch >= 0x20 && k.ch < 0x7f) {
            if (m_state.confirmInput.size() < sizeof(FORCE_WORD) + 8) {
                m_state.confirmInput.push_back(k.ch);
            }

            const std::string palabra(FORCE_WORD);
            const std::string& s = m_state.confirmInput;
            if (s.size() > palabra.size() ||
                palabra.compare(0, s.size(), s) != 0) {
                m_state.showConfirm = false;
                m_state.confirmKind = ConfirmKind::None;
                m_state.confirmInput.clear();
                setStatus("Cancelado: la palabra no coincide", true);
            }
        }
        return;
    }

    if (k.type == KeyType::Escape) {
        m_state.showConfirm = false;
        m_state.confirmKind = ConfirmKind::None;
        setStatus("Cancelado");
        return;
    }
    if (k.type == KeyType::Enter || (k.isChar() && (k.ch == 's' || k.ch == 'S' || k.ch == 'y' ||
                                                   k.ch == 'Y'))) {
        if (m_state.confirmKind == ConfirmKind::ForceKill) {
            m_state.confirmPhase = ConfirmPhase::ConfirmWord;
            m_state.confirmInput.clear();
        } else {
            executeConfirmedAction();
        }
        return;
    }
    if (k.isChar() && (k.ch == 'n' || k.ch == 'N')) {
        m_state.showConfirm = false;
        m_state.confirmKind = ConfirmKind::None;
        setStatus("Cancelado");
    }
}

void App::handleNiceMenuKey(const Key& k) {
    switch (k.type) {
        case KeyType::Up:
            m_state.niceIndex = std::max(0, m_state.niceIndex - 1);
            return;
        case KeyType::Down:
            m_state.niceIndex = std::min(NICE_CHOICE_COUNT - 1, m_state.niceIndex + 1);
            return;
        case KeyType::Escape:
            m_state.showNiceMenu = false;
            setStatus("Cambio de prioridad cancelado");
            return;
        case KeyType::Enter: {
            const int   pid  = m_state.niceTargetPid;
            const int   nice = NICE_CHOICES[m_state.niceIndex];
            const char* nom  = processByPid(pid) != nullptr ? processByPid(pid)->shortName.c_str()
                                                            : "proceso";
            const std::string nombre = nom;

            m_state.showNiceMenu = false;

            std::string msg;
            if (setNice(pid, nice, msg)) {
                setStatus("Prioridad del proceso " + std::to_string(pid) + " (" + nombre +
                          ") cambiada a " + std::to_string(nice));
                m_nextRefreshMs = 0;
            } else {
                setStatus(msg, true);
            }
            return;
        }
        default:
            return;
    }
}

void App::handleFilterKey(const Key& k) {
    switch (k.type) {
        case KeyType::Enter:
            m_state.filter     = m_state.filterInput;
            m_state.filtering  = false;
            rebuildVisible();
            rebuildTree();

            m_state.selected = 0;
            m_state.scroll   = 0;
            m_state.selectedPid = -1;
            resolveSelection();
            setStatus(m_state.filter.empty()
                          ? std::string("Filtro quitado")
                          : "Filtro activo: \"" + m_state.filter + "\"");
            return;
        case KeyType::Escape:
            m_state.filtering  = false;
            m_state.filterInput = m_state.filter;
            setStatus("Filtro cancelado");
            return;
        case KeyType::Backspace:
            if (!m_state.filterInput.empty()) m_state.filterInput.pop_back();
            return;
        default:
            if (k.isChar() && k.ch >= 0x20 && k.ch < 0x7f) {
                m_state.filterInput.push_back(k.ch);

                m_state.filter = m_state.filterInput;
                rebuildVisible();
                rebuildTree();
                resolveSelection();
            }
            return;
    }
}

void App::handleNormalKey(const Key& k) {
    const int utiles = std::max(1, bodyHeight() - 1);

    if (k.type == KeyType::Up)   { navigate(-1); return; }
    if (k.type == KeyType::Down) { navigate(+1); return; }
    if (k.type == KeyType::PageUp) {
        if (m_state.mode != ViewMode::Threads && m_state.mode != ViewMode::History) {
            navigate(-utiles);
        }
        return;
    }
    if (k.type == KeyType::PageDown) {
        if (m_state.mode != ViewMode::Threads && m_state.mode != ViewMode::History) {
            navigate(+utiles);
        }
        return;
    }
    if (k.type == KeyType::Home) {
        if (m_state.mode != ViewMode::Threads && m_state.mode != ViewMode::History) {
            m_state.selected = 0;
            clampScroll();
        }
        return;
    }
    if (k.type == KeyType::End) {
        if (m_state.mode != ViewMode::Threads && m_state.mode != ViewMode::History) {
            m_state.selected = std::max(0, static_cast<int>(activeRows().size()) - 1);
            clampScroll();
        }
        return;
    }

    if (k.type == KeyType::Left)  { treeNavParent(); return; }
    if (k.type == KeyType::Right) { treeNavChild();  return; }

    if (!k.isChar()) {
        if (k.type == KeyType::Escape) {
            if (!m_state.filter.empty()) {
                m_state.filter.clear();
                rebuildVisible();
                rebuildTree();
                resolveSelection();
                setStatus("Filtro quitado");
            } else if (m_state.mode != ViewMode::Table) {
                setMode(ViewMode::Table);
                setStatus("Vista de tabla");
            }
        }
        return;
    }

    switch (k.ch) {

        case 'q':
        case 'Q':
            m_state.quit = true;
            return;

        case '+':
        case '=':
            adjustInterval(INTERVAL_STEP_MS);
            return;
        case '-':
        case '_':
            adjustInterval(-INTERVAL_STEP_MS);
            return;

        case 'c':
        case 'C':
            sortBy(SortKey::Cpu);
            return;
        case 'm':
        case 'M':
            sortBy(SortKey::Mem);
            return;
        case 'i':
        case 'I':
            sortBy(SortKey::Pid);
            return;
        case 'u':
        case 'U':
            sortBy(SortKey::User);
            return;

        case '/':
            m_state.filtering  = true;
            m_state.filterInput = m_state.filter;
            Input::drain();
            setStatus("Escribe el filtro y pulsa Enter (Esc cancela)");
            return;

        case 't':
        case 'T':
            if (m_state.mode == ViewMode::Tree) {
                setMode(ViewMode::Table);
                setStatus("Vista de tabla");
            } else {
                setMode(ViewMode::Tree);
                setStatus("Vista de arbol: padre -> hijo  (izquierda: padre, derecha: hijo)");
            }
            return;
        case 'h':
        case 'H': {
            const ProcessInfo* p = selectedProcess();
            if (p == nullptr) {
                setStatus("No hay ningun proceso seleccionado", true);
                return;
            }
            m_state.targetPid = p->pid;
            m_state.mode      = ViewMode::Threads;
            m_threads         = threadsOf(p->pid);
            setStatus("Hilos del proceso " + std::to_string(p->pid));
            return;
        }
        case 'g':
        case 'G': {
            const ProcessInfo* p = selectedProcess();
            if (p == nullptr) {
                setStatus("No hay ningun proceso seleccionado", true);
                return;
            }
            m_state.targetPid = p->pid;
            m_state.mode      = ViewMode::History;
            setStatus("Historico de CPU del proceso " + std::to_string(p->pid));
            return;
        }

        case 'k':
            openConfirm(ConfirmKind::Terminate);
            return;
        case 'K':
            openConfirm(ConfirmKind::ForceKill);
            return;
        case 's':
            openConfirm(ConfirmKind::Suspend);
            return;
        case 'r':
            openConfirm(ConfirmKind::Resume);
            return;
        case 'p':
        case 'P':
            openNiceMenu();
            return;

        case '?':
            toggleHelp();
            return;

        default:
            return;
    }
}

void App::handleKey(const Key& k) {
    if (m_showHelp) {
        if (k.type == KeyType::Escape || k.isChar()) {
            m_showHelp = false;
            Input::drain();
        }
        return;
    }
    if (m_state.showConfirm) {
        handleConfirmKey(k);
        return;
    }
    if (m_state.showNiceMenu) {
        handleNiceMenuKey(k);
        return;
    }
    if (m_state.filtering) {
        handleFilterKey(k);
        return;
    }
    handleNormalKey(k);
}

void App::render() {
    const int rows = m_renderer.rows();
    const int cols = m_renderer.cols();

    if (rows < MIN_ROWS || cols < MIN_COLS) {
        renderTooSmall(m_renderer, rows, cols);
        m_renderer.flush();
        return;
    }

    m_renderer.clear();
    renderHeader(m_renderer, m_snapshot, m_state);

    const int top    = bodyTop();
    const int altura = bodyHeight();

    const std::vector<const ProcessInfo*>& filas = activeRows();
    const int total   = static_cast<int>(filas.size());
    const int utiles  = std::max(1, altura - 1);
    m_state.rowsTotal = total;
    m_state.rowsFirst = (total == 0) ? 0 : std::min(m_state.scroll + 1, total);
    m_state.rowsLast  = (total == 0) ? 0 : std::min(m_state.scroll + utiles, total);

    switch (m_state.mode) {
        case ViewMode::Table:
            renderTable(m_renderer, filas, m_state.scroll, m_state.selected, top, altura,
                        m_state);
            break;

        case ViewMode::Tree:
            renderTree(m_renderer, m_treeRows, m_state.scroll, m_state.selected, top, altura,
                       m_state);
            break;

        case ViewMode::Threads: {
            const ProcessInfo* p = processByPid(m_state.targetPid);
            if (p == nullptr) {
                setMode(ViewMode::Table);
                setStatus("El proceso " + std::to_string(m_state.targetPid) +
                              " ya no existe", true);
                renderTable(m_renderer, m_visible, m_state.scroll, m_state.selected, top,
                            altura, m_state);
            } else {
                renderThreads(m_renderer, *p, m_threads, top, altura);
            }
            break;
        }

        case ViewMode::History: {
            const ProcessInfo* p = processByPid(m_state.targetPid);
            if (p == nullptr) {
                setMode(ViewMode::Table);
                setStatus("El proceso " + std::to_string(m_state.targetPid) +
                              " ya no existe", true);
                renderTable(m_renderer, m_visible, m_state.scroll, m_state.selected, top,
                            altura, m_state);
            } else {
                renderHistory(m_renderer, *p, top, altura);
            }
            break;
        }
    }

    renderFooter(m_renderer, m_state);

    if (m_showHelp) {
        const int lineas = 26, columnas = std::min(cols - 4, 62);
        const int t = std::max(0, (rows - lineas) / 2);
        const int l = std::max(0, (cols - columnas) / 2);
        m_renderer.fillRect(t, l, t + lineas - 1, l + columnas - 1, WHITE, BLACK);
        for (int c = l; c < l + columnas; ++c) {
            m_renderer.put(t, c, U'\u2500', CYAN, BLACK);
            m_renderer.put(t + lineas - 1, c, U'\u2500', CYAN, BLACK);
        }
        m_renderer.put(t, l, U'\u250C', CYAN, BLACK);
        m_renderer.put(t, l + columnas - 1, U'\u2510', CYAN, BLACK);
        m_renderer.put(t + lineas - 1, l, U'\u2514', CYAN, BLACK);
        m_renderer.put(t + lineas - 1, l + columnas - 1, U'\u2518', CYAN, BLACK);
        for (int y = t; y < t + lineas; ++y) {
            m_renderer.put(y, l, U'\u2502', CYAN, BLACK);
            m_renderer.put(y, l + columnas - 1, U'\u2502', CYAN, BLACK);
        }
        const char* ayuda[] = {
            "Balduino - monitor de procesos",
            "",
            "q          salir (tambien Ctrl-C)",
            "+ / -      intervalo de refresco",
            "c / m      ordenar por CPU / memoria",
            "i / u      ordenar por PID / usuario",
            "/          filtrar por nombre",
            "t          vista de arbol",
            "h          vista de hilos",
            "g          historico de CPU",
            "k          terminar (SIGTERM)",
            "K          terminar por la fuerza (SIGKILL)",
            "s          suspender (SIGSTOP)",
            "r          reanudar (SIGCONT)",
            "p          cambiar prioridad (nice)",
            "---",
            "Flechas para moverse por la lista:",
            "  arriba / abajo        una fila",
            "  Re Pag / Av Pag       una pantalla",
            "  Inicio / Fin          primero / ultimo",
            "  izquierda / derecha   solo en el arbol:",
            "                         padre / primer hijo",
            "",
            "Esc        atras / cancelar",
        };
        for (int i = 0; i < static_cast<int>(sizeof(ayuda) / sizeof(ayuda[0])); ++i) {
            m_renderer.putStr(t + 1 + i, l + 2,
                              padRight(ayuda[i], columnas - 4),
                              (i == 0)   ? BRIGHT_YELLOW
                              : (ayuda[i][0] == '-' && i > 0) ? BRIGHT_CYAN
                                                             : WHITE,
                              BLACK,
                              (i == 0) ? ATTR_BOLD : ATTR_NONE);
        }
    }

    if (m_state.showConfirm) {
        ProcessInfo dummy;
        dummy.pid       = m_state.confirmPid;
        dummy.shortName = m_state.confirmName;
        if (const ProcessInfo* real = processByPid(m_state.confirmPid)) dummy = *real;
        renderConfirm(m_renderer, m_state, dummy);
    }

    if (m_state.showNiceMenu) {
        ProcessInfo dummy;
        dummy.pid       = m_state.niceTargetPid;
        dummy.shortName = "?";
        if (const ProcessInfo* real = processByPid(m_state.niceTargetPid)) dummy = *real;
        renderNiceMenu(m_renderer, m_state, dummy);
    }

    m_renderer.flush();
}

void App::run() {
    Terminal::init();

    int rows = 0, cols = 0;
    Terminal::getSize(rows, cols);
    m_renderer.resize(rows, cols);
#ifdef _WIN32
    m_state.limitedMode = true;
#else
    m_state.limitedMode = (geteuid() != 0);
#endif

    refresh();
    render();
    m_nextRefreshMs = nowMs() + m_state.intervalMs;

    if (m_state.limitedMode) {
        setStatus("Modo limitado: se necesitan privilegios root para actuar sobre procesos "
                  "de otros usuarios");
    }

    while (!m_state.quit) {

        const long long restante = m_nextRefreshMs - nowMs();
        const int timeout = static_cast<int>(std::max(0LL, std::min<long long>(
                                             restante, m_state.intervalMs)));
        const Key key = Input::readKey(timeout);

        if (Terminal::shouldQuit()) m_state.quit = true;

        if (Terminal::hasResized()) {
            Terminal::getSize(rows, cols);
            m_renderer.resize(rows, cols);
            clampScroll();

            render();
        }

        if (!key.isNone()) {
            handleKey(key);
            render();
        }

        if (nowMs() >= m_nextRefreshMs) {
            refresh();
            render();
            m_nextRefreshMs = nowMs() + m_state.intervalMs;
        }
    }

    Terminal::restore();
}

}
