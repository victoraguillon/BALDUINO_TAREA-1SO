#include "ProcStats.h"

#include "Utils.h"

#include <algorithm>
#include <unordered_map>

namespace Balduino {

void computeCpu(Snapshot& current, const Snapshot& previous) {

    std::unordered_map<int, const ProcessInfo*> prevByPid;
    prevByPid.reserve(previous.processes.size());
    for (const ProcessInfo& p : previous.processes) prevByPid.emplace(p.pid, &p);

    const std::uint64_t deltaTotal =
        (current.totalCpuJiffies > previous.totalCpuJiffies)
            ? current.totalCpuJiffies - previous.totalCpuJiffies : 0;

    const int cpus = (current.summary.numCpus > 0) ? current.summary.numCpus : 1;

    for (ProcessInfo& p : current.processes) {
        const auto it = prevByPid.find(p.pid);
        if (it == prevByPid.end()) {

            p.cpuPercent = 0.0;
            continue;
        }

        const ProcessInfo* prev    = it->second;
        const std::uint64_t prevTicks = prev->utime + prev->stime;
        const std::uint64_t currTicks = p.utime + p.stime;

        if (deltaTotal > 0 && currTicks >= prevTicks) {
            const double used = static_cast<double>(currTicks - prevTicks);
p.cpuPercent = used * 100.0 * static_cast<double>(cpus) /
                           static_cast<double>(deltaTotal);

            if (p.cpuPercent < 0.0) p.cpuPercent = 0.0;
        } else {
            p.cpuPercent = 0.0;
        }
    }

    for (ProcessInfo& p : current.processes) {
        const auto it = prevByPid.find(p.pid);
        if (it != prevByPid.end()) {
            p.cpuHistory = it->second->cpuHistory;
        }
        p.cpuHistory.push_back(p.cpuPercent);
        while (p.cpuHistory.size() > HISTORY_SIZE) p.cpuHistory.pop_front();
    }

    const std::uint64_t deltaIdle =
        (current.idleCpuJiffies > previous.idleCpuJiffies)
            ? current.idleCpuJiffies - previous.idleCpuJiffies : 0;
    const std::uint64_t deltaIowait =
        (current.iowaitCpuJiffies > previous.iowaitCpuJiffies)
            ? current.iowaitCpuJiffies - previous.iowaitCpuJiffies : 0;

    if (deltaTotal > 0) {
        const double busy = static_cast<double>(deltaTotal) -
                            static_cast<double>(deltaIdle) -
                            static_cast<double>(deltaIowait);
        current.summary.globalCpuPercent = busy * 100.0 / static_cast<double>(deltaTotal);
        if (current.summary.globalCpuPercent < 0.0) current.summary.globalCpuPercent = 0.0;
        if (current.summary.globalCpuPercent > 100.0) current.summary.globalCpuPercent = 100.0;
    } else {
        current.summary.globalCpuPercent = 0.0;
    }
}

void computeMem(Snapshot& s) {
    const long memTotal = s.summary.memTotal;
    for (ProcessInfo& p : s.processes) {
        p.memPercent = (memTotal > 0)
                           ? static_cast<double>(p.rssBytes) * 100.0 /
                                 static_cast<double>(memTotal)
                           : 0.0;
    }
}

void classifyProcesses(Snapshot& s) {
    SystemSummary& sum = s.summary;
    sum.total     = static_cast<int>(s.processes.size());
    sum.running   = 0;
    sum.sleeping  = 0;
    sum.stopped   = 0;
    sum.zombies   = 0;

    for (ProcessInfo& p : s.processes) {
        p.isZombie    = (p.state == 'Z');
        p.isBlockedIO = (p.state == 'D');

        switch (p.state) {
            case 'R': ++sum.running;  break;
            case 'T': ++sum.stopped;  break;
            case 'Z': ++sum.zombies;  break;
            case 'X':
            case 'x': ++sum.zombies;  break;
            default:   ++sum.sleeping; break;
        }
    }
}

}
