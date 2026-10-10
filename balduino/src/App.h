#ifndef BALDUINO_APP_H
#define BALDUINO_APP_H

#include "Input.h"
#include "Renderer.h"
#include "Types.h"

#include <string>
#include <vector>

namespace Balduino {

class App {
public:
    void run();

private:
    void refresh();
    void render();
    void rebuildVisible();
    void rebuildTree();

    const std::vector<const ProcessInfo*>& activeRows() const;

    void clampScroll();

    void resolveSelection();

    void navigate(int delta);
    void treeNavParent();
    void treeNavChild();
    void setMode(ViewMode mode);

    const ProcessInfo* selectedProcess() const;
    const ProcessInfo* processByPid(int pid) const;
    std::vector<ThreadInfo> threadsOf(int pid) const;

    void handleKey(const Key& k);
    void handleNormalKey(const Key& k);
    void handleFilterKey(const Key& k);
    void handleConfirmKey(const Key& k);
    void handleNiceMenuKey(const Key& k);

    void adjustInterval(int deltaMs);
    void sortBy(SortKey key);
    void setStatus(const std::string& msg, bool isError = false);
    void openConfirm(ConfirmKind kind);
    void executeConfirmedAction();
    void openNiceMenu();
    void toggleHelp();

    int bodyTop() const;
    int bodyHeight() const;

    AppState  m_state;
    Renderer  m_renderer;
    Snapshot  m_snapshot;
    bool      m_haveSnapshot = false;

    std::vector<const ProcessInfo*> m_visible;
    std::vector<TreeRow>            m_treeRows;
    std::vector<const ProcessInfo*> m_treeProcs;
    std::vector<ThreadInfo>         m_threads;

    long long m_nextRefreshMs = 0;
    bool      m_showHelp = false;
};

}

#endif
