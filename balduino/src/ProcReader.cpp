#include "ProcReader.h"

#include "Utils.h"

#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <utility>
#include <unordered_map>

namespace Balduino {
namespace {

std::unordered_map<uid_t, std::string>& userCache() {
    static std::unordered_map<uid_t, std::string> cache;
    return cache;
}

bool parseStatLine(const std::string& line, int& pidOut, std::string& commOut,
                   std::vector<std::string>& fieldsOut) {
    const std::size_t open = line.find('(');
    const std::size_t close = line.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) return false;

    const std::string pidPart = line.substr(0, open);
    if (!isNumeric(trim(pidPart))) return false;
    pidOut = std::atoi(trim(pidPart).c_str());

    commOut = line.substr(open + 1, close - open - 1);

    if (close + 1 >= line.size()) return false;
    fieldsOut = splitWhitespace(line.substr(close + 1));
    return true;
}

long toLong(const std::vector<std::string>& fields, std::size_t index, long fallback = 0) {
    if (index >= fields.size()) return fallback;
    const std::string& s = fields[index];
    if (!isNumeric(s)) return fallback;
    return std::atol(s.c_str());
}

std::string readCommandLine(int pid, const std::string& comm) {
    std::string cmd;
    if (readFile("/proc/" + std::to_string(pid) + "/cmdline", cmd)) {
        for (char& c : cmd) {
            if (c == '\0') c = ' ';
        }
        cmd = trim(cmd);
        if (!cmd.empty()) return cmd;
    }
    return "[" + comm + "]";
}

long readResidentBytes(int pid, const std::vector<std::string>& fields) {
    std::string statm;
    if (readFile("/proc/" + std::to_string(pid) + "/statm", statm)) {
        const std::vector<std::string> parts = splitWhitespace(trim(statm));
        if (parts.size() >= 2 && isNumeric(parts[1])) {
            return std::atol(parts[1].c_str()) * pageSize();
        }
    }
    return toLong(fields, 21) * pageSize();
}

}

std::string resolveUser(uid_t uid) {
    auto&       cache = userCache();
    const auto  it    = cache.find(uid);
    if (it != cache.end()) return it->second;

    std::string name = std::to_string(uid);
    if (const struct passwd* pw = ::getpwuid(uid); pw != nullptr && pw->pw_name != nullptr) {
        name = pw->pw_name;
    }
    cache.emplace(uid, name);
    return name;
}

ReadStatus readProcess(int pid, ProcessInfo& out) {
    out = ProcessInfo{};
    out.pid = pid;

    const std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
    std::string       line;
    if (!readFile(statPath, line)) {

        return (errno == EACCES || errno == EPERM) ? ReadStatus::NoAccess : ReadStatus::Gone;
    }

    int                     parsedPid = 0;
    std::string             comm;
    std::vector<std::string> fields;
    if (!parseStatLine(line, parsedPid, comm, fields)) {
        out.noAccess = true;
        return ReadStatus::NoAccess;
    }

    if (fields.empty()) {
        out.noAccess = true;
        return ReadStatus::NoAccess;
    }

    out.pid        = parsedPid;
    out.state      = fields[0].empty() ? '?' : fields[0][0];
    out.ppid       = static_cast<int>(toLong(fields, 1));
    out.utime      = static_cast<std::uint64_t>(toLong(fields, 11));
    out.stime      = static_cast<std::uint64_t>(toLong(fields, 12));
    out.priority   = static_cast<int>(toLong(fields, 15));
    out.nice       = static_cast<int>(toLong(fields, 16));
    out.threads    = static_cast<int>(toLong(fields, 17, 1));
    out.starttime  = static_cast<std::uint64_t>(toLong(fields, 19));
    out.shortName  = comm;
    out.rssBytes   = readResidentBytes(parsedPid, fields);
    out.isZombie   = (out.state == 'Z');
    out.isBlockedIO= (out.state == 'D');

    struct stat st{};
    if (::stat(statPath.c_str(), &st) == 0) {
        out.uid  = st.st_uid;
        out.user = resolveUser(st.st_uid);
    }

    out.command = readCommandLine(parsedPid, comm);
    return ReadStatus::Ok;
}

std::vector<ThreadInfo> readThreads(int pid) {
    std::vector<ThreadInfo> threads;

    const std::string taskPath = "/proc/" + std::to_string(pid) + "/task";
    DIR* const         dir     = ::opendir(taskPath.c_str());
    if (dir == nullptr) return threads;

    const struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (!isNumeric(entry->d_name)) continue;

        const int       tid  = std::atoi(entry->d_name);
        const std::string line =
            readFileOr(taskPath + "/" + entry->d_name + "/stat");

        int                     parsedPid = 0;
        std::string             comm;
        std::vector<std::string> fields;
        if (!parseStatLine(line, parsedPid, comm, fields) || fields.empty()) continue;

        ThreadInfo info;
        info.tid      = tid;
        info.name     = comm;
        info.state    = fields[0].empty() ? '?' : fields[0][0];
        info.priority = static_cast<int>(toLong(fields, 15));
        info.nice     = static_cast<int>(toLong(fields, 16));
        info.utime    = static_cast<std::uint64_t>(toLong(fields, 11));
        info.stime    = static_cast<std::uint64_t>(toLong(fields, 12));
        threads.push_back(std::move(info));
    }
    ::closedir(dir);

    std::stable_sort(threads.begin(), threads.end(),
                     [pid](const ThreadInfo& a, const ThreadInfo& b) {
                         const int ap = (a.tid == pid) ? 0 : 1;
                         const int bp = (b.tid == pid) ? 0 : 1;
                         if (ap != bp) return ap < bp;
                         return a.tid < b.tid;
                     });
    return threads;
}

SystemSummary readSystemSummary(std::uint64_t& totalJiffies,
                               std::uint64_t& idleJiffies,
                               std::uint64_t& iowaitJiffies) {
    SystemSummary summary;
    summary.numCpus = numCpus();

    if (std::string stat; readFile("/proc/stat", stat)) {
        const std::size_t firstLine = stat.find('\n');
        const std::string cpuLine  = stat.substr(0, firstLine);
        const std::vector<std::string> f = splitWhitespace(cpuLine);

        if (f.size() >= 5 && f[0] == "cpu") {
            std::uint64_t total = 0;

            for (std::size_t i = 1; i < f.size() && i <= 8; ++i) {
                if (isNumeric(f[i])) total += std::strtoull(f[i].c_str(), nullptr, 10);
            }
            totalJiffies  = total;
            idleJiffies   = isNumeric(f[4]) ? std::strtoull(f[4].c_str(), nullptr, 10) : 0;
            iowaitJiffies = (f.size() > 5 && isNumeric(f[5]))
                                ? std::strtoull(f[5].c_str(), nullptr, 10) : 0;
        }
    }

    if (std::string mem; readFile("/proc/meminfo", mem)) {
        long total = 0, free = 0, available = 0;
        for (const std::string& line : split(mem, '\n')) {
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            const std::string key = trim(line.substr(0, colon));
            const std::vector<std::string> val = splitWhitespace(line.substr(colon + 1));
            if (val.empty() || !isNumeric(val[0])) continue;
            const long kb = std::atol(val[0].c_str());
            if (key == "MemTotal")          total     = kb;
            else if (key == "MemFree")       free      = kb;
            else if (key == "MemAvailable")  available = kb;
        }

        summary.memTotal     = total * 1024L;
        summary.memFree      = free * 1024L;
        summary.memAvailable = available * 1024L;
        summary.memUsed      = summary.memTotal - summary.memAvailable;
    }

    if (std::string load; readFile("/proc/loadavg", load)) {
        const std::vector<std::string> parts = splitWhitespace(load);
        if (parts.size() >= 3) {
            summary.load1  = std::atof(parts[0].c_str());
            summary.load5  = std::atof(parts[1].c_str());
            summary.load15 = std::atof(parts[2].c_str());
        }
    }

    if (std::string up; readFile("/proc/uptime", up)) {
        const std::vector<std::string> parts = splitWhitespace(up);
        if (!parts.empty()) summary.uptimeSeconds = std::atof(parts[0].c_str());
    }

    return summary;
}

Snapshot readSnapshot() {
    Snapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();

    snap.summary = readSystemSummary(snap.totalCpuJiffies, snap.idleCpuJiffies,
                                    snap.iowaitCpuJiffies);

    DIR* const dir = ::opendir("/proc");
    if (dir == nullptr) return snap;

    std::vector<int> pids;
    const struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (!isNumeric(entry->d_name)) continue;
        pids.push_back(std::atoi(entry->d_name));
    }
    ::closedir(dir);

    snap.processes.reserve(pids.size());
    for (const int pid : pids) {
        ProcessInfo info;
        const ReadStatus status = readProcess(pid, info);
        if (status == ReadStatus::Gone) continue;

        snap.processes.push_back(std::move(info));
    }

    return snap;
}

}
