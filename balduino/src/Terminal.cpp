#include "Terminal.h"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <cstring>

namespace Balduino {
namespace {

struct termios g_original{};
bool          g_rawActive = false;
bool          g_restored  = false;

volatile sig_atomic_t g_resized = 0;
volatile sig_atomic_t g_quit    = 0;

void writeAll(const char* s) {
    const std::size_t len = std::strlen(s);
    if (len == 0) return;
    const ssize_t written = ::write(STDOUT_FILENO, s, len);
    (void)written;
}

void onWinch(int) { g_resized = 1; }
void onQuit(int)  { g_quit    = 1; }

void onFatal(int sig) {
    Terminal::restoreNow();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

}

void Terminal::init() {
    if (g_restored) return;

    if (::tcgetattr(STDIN_FILENO, &g_original) == 0) {
        struct termios raw = g_original;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= CS8;
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) g_rawActive = true;
    }

    writeAll("\x1b[?1049h");
    writeAll("\x1b[?25l");
    writeAll("\x1b[2J");
    writeAll("\x1b[H");

    std::atexit(Terminal::restore);

    std::signal(SIGWINCH, onWinch);
    std::signal(SIGINT,   onQuit);
    std::signal(SIGTERM,  onQuit);
    std::signal(SIGSEGV,  onFatal);
    std::signal(SIGBUS,   onFatal);
    std::signal(SIGFPE,   onFatal);
    std::signal(SIGILL,   onFatal);

    std::signal(SIGPIPE, SIG_IGN);
}

void Terminal::restoreNow() {
    writeAll("\x1b[0m");
    writeAll("\x1b[?25h");
    writeAll("\x1b[?1049l");
    if (g_rawActive) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_original);
        g_rawActive = false;
    }
}

void Terminal::restore() {
    if (g_restored) return;
    g_restored = true;
    restoreNow();
}

void Terminal::getSize(int& rows, int& cols) {
    rows = 24;
    cols = 80;
    struct winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
}

bool Terminal::hasResized() {
    if (g_resized == 0) return false;
    g_resized = 0;
    return true;
}

bool Terminal::shouldQuit() {
    if (g_quit == 0) return false;
    g_quit = 0;
    return true;
}

}
