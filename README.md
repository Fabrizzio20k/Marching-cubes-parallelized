# Marching-cubes-parallelized

Implementación secuencial y paralela (MPI) del algoritmo Marching Cubes para la extracción de iso-superficies a partir de campos escalares 3D.

## Estructura

```
src/
  common/cCases.h        Tablas de casos, Vec3 e interpolación
  sequential/main.cpp    Versión secuencial
  parallelized/main.cpp  Versión MPI (Slab Decomposition + MPI-IO)
run.sh                   Script de compilación y ejecución
```

## Requisitos

- Compilador C++17 (`g++`)
- Implementación de MPI (`mpic++`, `mpirun`), por ejemplo Open MPI o MPICH

## Uso

```bash
./run.sh seq [N] [output.ply]
./run.sh par [num_procesos] [N] [output.ply]
```

Ejemplos:

```bash
./run.sh seq 128
./run.sh par 8 512 toro.ply
```

`N` es el número de celdas por lado del volumen (N x N x N celdas). Ambas versiones reportan tiempo de cómputo, tiempo de escritura, FLOPs y GFLOPS, y generan un archivo PLY binario idéntico entre versiones.

## Diseño de la versión paralela

- **Slab Decomposition:** el eje Z se divide equitativamente entre los `p` procesos, sin comunicación durante el cálculo.
- **MPI_Exscan / MPI_Allreduce:** cálculo de offsets globales de vértices y triángulos (suma de prefijos paralela).
- **MPI-IO (`MPI_File_write_at_all`):** escritura concurrente del archivo PLY sin condiciones de carrera.
