#include "Input.h"

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace Balduino {
namespace {

constexpr int MAX_SEQUENCE = 16;

int readByte(int timeoutMs) {
    struct pollfd pfd{};
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    const int ready = ::poll(&pfd, 1, timeoutMs);
    if (ready <= 0) return -1;

    unsigned char c = 0;
    const ssize_t  n = ::read(STDIN_FILENO, &c, 1);
    if (n != 1) return -1;
    return static_cast<int>(c);
}

Key decodeSequence(const std::string& seq) {
    if (seq.size() == 1) return { KeyType::Escape, '\0' };

    if (seq.size() >= 3 && seq[1] == '[') {
        const char final = seq[seq.size() - 1];
        switch (final) {
            case 'A': return { KeyType::Up, '\0' };
            case 'B': return { KeyType::Down, '\0' };
            case 'C': return { KeyType::Right, '\0' };
            case 'D': return { KeyType::Left, '\0' };
            case 'H': return { KeyType::Home, '\0' };
            case 'F': return { KeyType::End, '\0' };
            case '~': break;
            default:  return { KeyType::None, '\0' };
        }

        const std::string digits = seq.substr(2, seq.size() - 3);
        if (digits.empty()) return { KeyType::None, '\0' };
        if (digits == "1" || digits == "7") return { KeyType::Home, '\0' };
        if (digits == "4" || digits == "8") return { KeyType::End, '\0' };
        if (digits == "5") return { KeyType::PageUp, '\0' };
        if (digits == "6") return { KeyType::PageDown, '\0' };
        if (digits == "3") return { KeyType::Delete, '\0' };
        return { KeyType::None, '\0' };
    }
    return { KeyType::None, '\0' };
}

}

Key Input::readKey(int timeoutMs) {
    const int first = readByte(timeoutMs);
    if (first < 0) return { KeyType::None, '\0' };

    if (first == 0x03) return { KeyType::Char, 'q' };

    if (first != 0x1b) {
        switch (first) {
            case '\r':
            case '\n': return { KeyType::Enter, '\0' };
            case 0x7f:
            case 0x08:
                return { KeyType::Backspace, '\0' };
            case '\t':  return { KeyType::Tab, '\0' };
            default: break;
        }
        if (first < 0x20) return { KeyType::Char, '\0' };
        return { KeyType::Char, static_cast<char>(first) };
    }

    std::string seq;
    seq.push_back(static_cast<char>(0x1b));

    int next = readByte(20);
    if (next < 0) return { KeyType::Escape, '\0' };
    seq.push_back(static_cast<char>(next));

    while (static_cast<int>(seq.size()) < MAX_SEQUENCE) {
        if (next == '[') {
            next = readByte(20);
            if (next < 0) break;
            seq.push_back(static_cast<char>(next));

            if ((next >= '0' && next <= '9') || next == ';') {
                next = readByte(20);
                if (next < 0) break;
                seq.push_back(static_cast<char>(next));
                continue;
            }
            break;
        }
        if (next == 'O') {
            next = readByte(20);
            if (next < 0) break;
            seq.push_back(static_cast<char>(next));
            switch (next) {
                case 'A': return { KeyType::Up, '\0' };
                case 'B': return { KeyType::Down, '\0' };
                case 'C': return { KeyType::Right, '\0' };
                case 'D': return { KeyType::Left, '\0' };
                case 'H': return { KeyType::Home, '\0' };
                case 'F': return { KeyType::End, '\0' };
                default:  return { KeyType::None, '\0' };
            }
        }
        break;
    }

    return decodeSequence(seq);
}

void Input::drain() {
    struct pollfd pfd{};
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    while (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) != 0) {
        unsigned char discard[256];
        const ssize_t  n = ::read(STDIN_FILENO, discard, sizeof(discard));
        if (n <= 0) break;
    }
}

}
