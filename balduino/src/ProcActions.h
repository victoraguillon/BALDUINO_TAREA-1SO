#ifndef BALDUINO_PROCACTIONS_H
#define BALDUINO_PROCACTIONS_H

#include <string>

namespace Balduino {

bool processExists(int pid, std::string& msg);

bool checkPermission(int pid, std::string& msg);

bool sendSignal(int pid, int sig, std::string& msg);

bool terminate(int pid, std::string& msg);
bool forceTerminate(int pid, std::string& msg);
bool suspend(int pid, std::string& msg);
bool resume(int pid, std::string& msg);

bool setNice(int pid, int nice, std::string& msg);
int  getNice(int pid);

}

#endif
