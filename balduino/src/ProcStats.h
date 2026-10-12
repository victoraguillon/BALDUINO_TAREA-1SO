#ifndef BALDUINO_PROCSTATS_H
#define BALDUINO_PROCSTATS_H

#include "Types.h"

namespace Balduino {

void computeCpu(Snapshot& current, const Snapshot& previous);

void computeMem(Snapshot& s);

void classifyProcesses(Snapshot& s);

}

#endif
