#!/bin/bash
#SBATCH --job-name=mc512
#SBATCH --partition=standard
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --output=mc_%j.out
#SBATCH --error=mc_%j.err

module load gnu12
module load openmpi4

N=${N:-512}
REPS=${REPS:-3}
PROCS=${PROCS:-"1 2 4 8 16"}
RESULTS="results_${SLURM_JOB_ID:-local}.csv"
TMPPLY="/tmp/mc_bench_$$.ply"

if [ ! -x build/mc_seq ] || [ ! -x build/mc_par ]; then
    echo "ERROR: binarios no encontrados, compila primero con ./compile.sh en el nodo de login" >&2
    exit 1
fi

echo "version,p,N,vertices,triangulos,t_computo,t_io,t_total,t_comp_max,t_comp_min,flops,gflops" > "$RESULTS"

for r in $(seq "$REPS"); do
    echo "=== secuencial N=$N rep=$r ==="
    OUT=$(build/mc_seq "$N" "$TMPPLY")
    echo "$OUT"
    echo "$OUT" | grep '^CSV,' | sed 's/^CSV/seq/' >> "$RESULTS"
done

for p in $PROCS; do
    for r in $(seq "$REPS"); do
        echo "=== mpi p=$p N=$N rep=$r ==="
        OUT=$(mpirun -np "$p" build/mc_par "$N" "$TMPPLY")
        echo "$OUT"
        echo "$OUT" | grep '^CSV,' | sed 's/^CSV/mpi/' >> "$RESULTS"
    done
done

rm -f "$TMPPLY"
echo "Resultados guardados en $RESULTS"
