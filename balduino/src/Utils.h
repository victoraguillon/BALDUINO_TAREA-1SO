#ifndef BALDUINO_UTILS_H
#define BALDUINO_UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace Balduino {

std::string formatBytes(long bytes);
std::string formatCpuTime(std::uint64_t jiffies);
std::string formatUptime(double seconds);
std::string formatPercent(double value, int decimals = 1);

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::string> splitWhitespace(const std::string& s);
std::string toLower(const std::string& s);
bool        isNumeric(const std::string& s);

std::string errnoMessage(int err);

long pageSize();
long clockTicks();
int  numCpus();

bool        readFile(const std::string& path, std::string& out);
std::string readFileOr(const std::string& path, const std::string& fallback = std::string());

std::string padRight(const std::string& s, int width);
std::string padLeft(const std::string& s, int width);

std::vector<char32_t> utf8Decode(const std::string& s);

}

#endif
