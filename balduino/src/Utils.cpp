#include "Utils.h"

#include "Types.h"

#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace Balduino {

const char* toString(ViewMode mode) {
    switch (mode) {
        case ViewMode::Table:   return "Tabla";
        case ViewMode::Tree:    return "Arbol";
        case ViewMode::Threads: return "Hilos";
        case ViewMode::History: return "Hist";
        default:                return "?";
    }
}

const char* toString(SortKey key) {
    switch (key) {
        case SortKey::Cpu:  return "CPU";
        case SortKey::Mem:  return "MEM";
        case SortKey::Pid:  return "PID";
        case SortKey::User: return "NI";
        default:            return "?";
    }
}

const char* toString(ConfirmKind kind) {
    switch (kind) {
        case ConfirmKind::Terminate: return "terminar";
        case ConfirmKind::ForceKill: return "terminar por la fuerza";
        case ConfirmKind::Suspend:   return "suspender";
        case ConfirmKind::Resume:    return "reanudar";
        case ConfirmKind::SetNice:   return "cambiar prioridad";
        default:                     return "ninguna";
    }
}

namespace {

std::size_t utf8SafeCut(const std::string& s, std::size_t maxBytes) {
    if (s.size() <= maxBytes) return s.size();
    std::size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
    return cut;
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}

std::string formatBytes(long bytes) {
    char buf[32];
    if (bytes < 0) bytes = 0;

    const long kb = 1024L;
    const long mb = kb * 1024L;
    const long gb = mb * 1024L;
    const long tb = gb * 1024L;

    double value    = 0.0;
    const char* unit = "B";

    if (bytes >= tb)      { value = static_cast<double>(bytes) / tb; unit = "T"; }
    else if (bytes >= gb) { value = static_cast<double>(bytes) / gb; unit = "G"; }
    else if (bytes >= mb) { value = static_cast<double>(bytes) / mb; unit = "M"; }
    else if (bytes >= kb) { value = static_cast<double>(bytes) / kb; unit = "K"; }
    else                  { value = static_cast<double>(bytes);      unit = "B"; }

    if (unit[0] == 'B') {
        std::snprintf(buf, sizeof(buf), "%ld%s", bytes, unit);
    } else if (value >= 100.0) {
        std::snprintf(buf, sizeof(buf), "%ld%s", static_cast<long>(value), unit);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f%s", value, unit);
    }
    return std::string(buf);
}

std::string formatCpuTime(std::uint64_t jiffies) {
    const long ticks = clockTicks();
    if (ticks <= 0) return std::string("0:00.00");

    const long long centis = static_cast<long long>(jiffies * 100ULL / static_cast<std::uint64_t>(ticks));
    const long long hours  = centis / 360000;
    const long long mins   = (centis / 6000) % 60;
    const long long secs   = (centis / 100) % 60;
    const long long cs     = centis % 100;

    char buf[40];
    if (hours > 0) {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", hours, mins, secs);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld.%02lld", mins, secs, cs);
    }
    return std::string(buf);
}

std::string formatUptime(double seconds) {
    char buf[64];
    const long long total = static_cast<long long>(seconds);
    const long long d = total / 86400;
    const long long h = (total % 86400) / 3600;
    const long long m = (total % 3600) / 60;
    const long long s = total % 60;

    if (d > 0)      std::snprintf(buf, sizeof(buf), "%lldd%lldh", d, h);
    else if (h > 0) std::snprintf(buf, sizeof(buf), "%lldh%lldm", h, m);
    else if (m > 0) std::snprintf(buf, sizeof(buf), "%lldm%llds", m, s);
    else            std::snprintf(buf, sizeof(buf), "%llds", s);
    return std::string(buf);
}

std::string formatPercent(double value, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return std::string(buf);
}

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() && isSpace(s[begin])) ++begin;
    std::size_t end = s.size();
    while (end > begin && isSpace(s[end - 1])) --end;
    return s.substr(begin, end - begin);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string              current;
    for (char c : s) {
        if (c == delim) {
            out.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(current);
    return out;
}

std::vector<std::string> splitWhitespace(const std::string& s) {
    std::vector<std::string> out;
    std::string              current;
    for (char c : s) {
        if (isSpace(c)) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

std::string toLower(const std::string& s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool isNumeric(const std::string& s) {
    if (s.empty()) return false;
    std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

std::string errnoMessage(int err) {
    char        buf[256];
    const char* msg = std::strerror(err);
    std::snprintf(buf, sizeof(buf), "%s (%d)", msg != nullptr ? msg : "error desconocido", err);
    return std::string(buf);
}

long pageSize() {
    static const long value = [] {
        const long v = sysconf(_SC_PAGESIZE);
        return v > 0 ? v : 4096L;
    }();
    return value;
}

long clockTicks() {
    static const long value = [] {
        const long v = sysconf(_SC_CLK_TCK);
        return v > 0 ? v : 100L;
    }();
    return value;
}

int numCpus() {
    static const int value = [] {
        const long v = sysconf(_SC_NPROCESSORS_ONLN);
        return v > 0 ? static_cast<int>(v) : 1;
    }();
    return value;
}

bool readFile(const std::string& path, std::string& out) {
    out.clear();
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char    buf[4096];
    ssize_t n;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, static_cast<std::size_t>(n));
    }
    const int saved = errno;
    ::close(fd);

    if (n < 0) {
        if (saved == EINTR) return readFile(path, out);
        errno = saved;
        return false;
    }
    return true;
}

std::string readFileOr(const std::string& path, const std::string& fallback) {
    std::string data;
    if (!readFile(path, data)) return fallback;
    return data;
}

std::string padRight(const std::string& s, int width) {
    if (width <= 0) return std::string();
    const std::size_t cut = utf8SafeCut(s, static_cast<std::size_t>(width));
    std::string       out = s.substr(0, cut);
    out.append(static_cast<std::size_t>(width) - cut, ' ');
    return out;
}

std::string padLeft(const std::string& s, int width) {
    if (width <= 0) return std::string();
    const std::size_t cut = utf8SafeCut(s, static_cast<std::size_t>(width));

    std::string out(static_cast<std::size_t>(width) - cut, ' ');
    out.append(s, 0, cut);
    return out;
}

std::vector<char32_t> utf8Decode(const std::string& s) {
    std::vector<char32_t> out;
    out.reserve(s.size());

    const std::size_t n = s.size();
    std::size_t       i = 0;
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t           cp   = 0;
        int                extra = 0;

        if (c < 0x80) {
            cp    = c;
            extra = 0;
        } else if ((c & 0xE0) == 0xC0) {
            cp    = c & 0x1Fu;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp    = c & 0x0Fu;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp    = c & 0x07u;
            extra = 3;
        } else {
            ++i;
            continue;
        }

        if (i + static_cast<std::size_t>(extra) >= n) {
            ++i;
            continue;
        }

        bool valid = true;
        for (int k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((cc & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (!valid) {
            ++i;
            continue;
        }

        out.push_back(cp);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return out;
}

}
