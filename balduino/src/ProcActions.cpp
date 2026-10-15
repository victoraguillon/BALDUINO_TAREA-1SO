#include "ProcActions.h"

#include "Utils.h"
#include <sys/resource.h>
#include <sys/types.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

namespace Balduino {
namespace {

int probe(int pid) {
    errno = 0;
    if (::kill(static_cast<pid_t>(pid), 0) == 0) return 0;
    return errno;
}

std::string describeErrno(int pid, int err) {
    switch (err) {
        case ESRCH:
            return "El proceso " + std::to_string(pid) + " ya no existe";
        case EPERM:
            return "Sin permiso para actuar sobre el proceso " + std::to_string(pid) +
                   " (hace falta ejecutarlo como root)";
        case EINVAL:
            return "Señal o prioridad no valida para el proceso " + std::to_string(pid);
        default:
            return "No se pudo actuar sobre el proceso " + std::to_string(pid) + ": " +
                   errnoMessage(err);
    }
}

}

bool processExists(int pid, std::string& msg) {
    if (pid <= 0) {
        msg = "PID no valido";
        return false;
    }
    const int err = probe(pid);
    if (err == 0) return true;

    msg = (err == EPERM) ? "El proceso " + std::to_string(pid) +
                               " existe pero pertenece a otro usuario"
                         : describeErrno(pid, err);
    return false;
}

bool checkPermission(int pid, std::string& msg) {
    if (pid <= 0) {
        msg = "PID no valido";
        return false;
    }
    const int err = probe(pid);
    if (err == 0) return true;
    if (err == EPERM) {
        msg = "Sin permiso: el proceso " + std::to_string(pid) +
              " pertenece a otro usuario (hace falta root)";
        return false;
    }
    msg = describeErrno(pid, err);
    return false;
}

bool sendSignal(int pid, int sig, std::string& msg) {
    if (!processExists(pid, msg)) return false;

    if (::kill(static_cast<pid_t>(pid), sig) == 0) return true;

    const int err = errno;
    msg = describeErrno(pid, err);
    return false;
}

bool terminate(int pid, std::string& msg) {

    return sendSignal(pid, SIGTERM, msg);
}

bool forceTerminate(int pid, std::string& msg) {

    return sendSignal(pid, SIGKILL, msg);
}

bool suspend(int pid, std::string& msg) {
    return sendSignal(pid, SIGSTOP, msg);
}

bool resume(int pid, std::string& msg) {
    return sendSignal(pid, SIGCONT, msg);
}

bool setNice(int pid, int nice, std::string& msg) {
    if (nice < -20 || nice > 19) {
        msg = "Prioridad fuera de rango: debe estar entre -20 y 19";
        return false;
    }
    if (!checkPermission(pid, msg)) return false;

    errno = 0;
    if (::setpriority(PRIO_PROCESS, static_cast<id_t>(pid), nice) == 0) return true;

    const int err = errno;

    if (err == EACCES || err == EPERM) {
        msg = "Sin permiso para cambiar la prioridad del proceso " + std::to_string(pid) +
              " a " + std::to_string(nice) + ". Sin privilegios solo se puede aumentar el "
              "valor de nice (reducir la prioridad); para mejorarla hace falta ejecutarlo "
              "como root";
        return false;
    }
    msg = describeErrno(pid, err);
    return false;
}

int getNice(int pid) {
    errno = 0;
    const int n = ::getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    if (n == -1 && errno != 0) return 0;
    return n;
}

}
