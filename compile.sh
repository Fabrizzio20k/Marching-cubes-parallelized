#!/bin/bash
set -e

module load gnu12 2>/dev/null || true
module load openmpi4 2>/dev/null || true

mkdir -p build
g++ -O3 -std=c++17 src/sequential/main.cpp -o build/mc_seq
mpic++ -O3 -std=c++17 src/parallelized/main.cpp -o build/mc_par
echo "Binarios generados en build/"
