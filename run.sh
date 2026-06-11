#!/bin/bash
set -e

BUILD_DIR="build"
CXX="${CXX:-g++}"
MPICXX="${MPICXX:-mpic++}"
CXXFLAGS="-O3 -std=c++17"

usage() {
    echo "Uso:"
    echo "  ./run.sh seq [N] [output.ply]"
    echo "  ./run.sh par [num_procesos] [N] [output.ply]"
    echo ""
    echo "Ejemplos:"
    echo "  ./run.sh seq 128"
    echo "  ./run.sh par 8 512 toro.ply"
    exit 1
}

[ $# -lt 1 ] && usage

MODE=$1
shift
mkdir -p "$BUILD_DIR"

case "$MODE" in
seq)
    N=${1:-64}
    OUT=${2:-output_seq.ply}
    echo "[*] Compilando version secuencial..."
    $CXX $CXXFLAGS src/sequential/main.cpp -o "$BUILD_DIR/mc_seq"
    echo "[*] Ejecutando: mc_seq N=$N -> $OUT"
    "$BUILD_DIR/mc_seq" "$N" "$OUT"
    ;;
par)
    NP=${1:-4}
    N=${2:-64}
    OUT=${3:-output_par.ply}
    echo "[*] Compilando version paralelizada (MPI)..."
    $MPICXX $CXXFLAGS src/parallelized/main.cpp -o "$BUILD_DIR/mc_par"
    echo "[*] Ejecutando: mpirun -np $NP mc_par N=$N -> $OUT"
    mpirun -np "$NP" "$BUILD_DIR/mc_par" "$N" "$OUT"
    ;;
*)
    usage
    ;;
esac
