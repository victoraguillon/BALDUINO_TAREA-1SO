#!/bin/bash
set -e
AQUI="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cmake -S "$AQUI" -B "$AQUI/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$AQUI/build" -j "$(nproc)"
echo
echo "Listo:  $AQUI/build/balduino"
echo "Ejecutar: $AQUI/build/balduino"
