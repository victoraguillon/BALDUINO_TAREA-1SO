#ifndef BALDUINO_TYPES_H
#define BALDUINO_TYPES_H

#include <sys/types.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Balduino {

constexpr std::size_t HISTORY_SIZE      = 60;
constexpr int         MIN_ROWS         = 20;
constexpr int         MIN_COLS         = 80;
constexpr int         DEFAULT_INTERVAL_MS = 1000;
constexpr int         MIN_INTERVAL_MS  = 250;
constexpr int         MAX_INTERVAL_MS  = 10000;
constexpr int         INTERVAL_STEP_MS = 250;
constexpr int         STATUS_DURATION_MS = 3000;

struct ProcessInfo {
    int           pid         = 0;
    int           ppid        = 0;
    uid_t         uid         = 0;
    std::string   user;
    char          state       = '?';
    double        cpuPercent  = 0.0;
    double        memPercent  = 0.0;
    long          rssBytes    = 0;
    int           threads     = 1;
    int           priority    = 0;
    int           nice        = 0;
    std::uint64_t utime       = 0;
    std::uint64_t stime       = 0;
    std::uint64_t starttime   = 0;
    std::string   command;
    std::string   shortName;
    bool          noAccess    = false;
    bool          isZombie    = false;
    bool          isBlockedIO = false;
    std::deque<double> cpuHistory;
};

struct ThreadInfo {
    int           tid      = 0;
    std::string   name;
    char          state    = '?';
    int           priority = 0;
    int           nice     = 0;
    std::uint64_t utime    = 0;
    std::uint64_t stime    = 0;
};

struct SystemSummary {
    int    total       = 0;
    int    running     = 0;
    int    sleeping    = 0;
    int    stopped     = 0;
    int    zombies     = 0;
    double load1       = 0.0;
    double load5       = 0.0;
    double load15      = 0.0;
    long   memTotal    = 0;
    long   memUsed     = 0;
    long   memFree     = 0;
    long   memAvailable= 0;
    double globalCpuPercent = 0.0;
    double uptimeSeconds     = 0.0;
    int    numCpus     = 1;
};

struct Snapshot {
    std::vector<ProcessInfo> processes;
    SystemSummary            summary;
    std::uint64_t            totalCpuJiffies  = 0;
    std::uint64_t            idleCpuJiffies   = 0;
    std::uint64_t            iowaitCpuJiffies = 0;
    std::chrono::steady_clock::time_point timestamp;
};

enum class ViewMode     { Table, Tree, Threads, History };
enum class SortKey      { Cpu, Mem, Pid, User };
enum class ConfirmKind  { None, Terminate, ForceKill, Suspend, Resume, SetNice };

enum class ConfirmPhase { Prompt, ConfirmWord };

const char* toString(ViewMode    mode);
const char* toString(SortKey     key);
const char* toString(ConfirmKind kind);

struct TreeRow {
    const ProcessInfo* proc  = nullptr;
    int                 depth = 0;
    bool                last  = false;
};

struct AppState {
    ViewMode    mode   = ViewMode::Table;
    SortKey     sort   = SortKey::Cpu;
    bool        sortAsc = false;

    std::string filter;
    bool        filtering = false;
    std::string filterInput;

    int         intervalMs = DEFAULT_INTERVAL_MS;

    int         selected   = 0;
    int         scroll     = 0;
    int         selectedPid = -1;
    int         targetPid  = -1;

    int         rowsTotal = 0;
    int         rowsFirst = 0;
    int         rowsLast  = 0;

    std::string statusMsg;
    std::chrono::steady_clock::time_point statusTime;
    bool        statusIsError = false;

    bool        showConfirm   = false;
    ConfirmKind confirmKind   = ConfirmKind::None;
    ConfirmPhase confirmPhase = ConfirmPhase::Prompt;
    std::string confirmInput;

    int         confirmPid    = -1;
    std::string confirmName;

    bool        showNiceMenu  = false;
    int         niceIndex     = 0;
    int         niceTargetPid = -1;

    bool        limitedMode = false;
    bool        quit        = false;
};

constexpr int NICE_CHOICES[]    = { -20, -10, -5, 0, 5, 10, 19 };
constexpr int NICE_CHOICE_COUNT = 7;

constexpr const char* FORCE_WORD = "FORZAR";

}

#endif
