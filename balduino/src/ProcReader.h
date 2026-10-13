#ifndef BALDUINO_PROCREADER_H
#define BALDUINO_PROCREADER_H

#include "Types.h"

#include <string>
#include <vector>

namespace Balduino {

enum class ReadStatus {
    Ok,
    Gone,
    NoAccess,
};

std::string resolveUser(uid_t uid);

ReadStatus readProcess(int pid, ProcessInfo& out);

std::vector<ThreadInfo> readThreads(int pid);

SystemSummary readSystemSummary(std::uint64_t& totalJiffies,
                               std::uint64_t& idleJiffies,
                               std::uint64_t& iowaitJiffies);

Snapshot readSnapshot();

}

#endif
