#include "../common/cCases.h"
#include <mpi.h>
#include <cstdint>
#include <cstring>
#include <sstream>

class MarchingCubesMPI
{
private:
    function<double(double, double, double)> f;
    vector<float> vertexData;
    long long flops;

    static constexpr long long EVAL_FLOPS = 10;
    static constexpr long long INTERP_FLOPS = 15;

    void processCell(double x1, double y1, double z1, double x2, double y2,
                     double z2)
    {
        Vec3 cubeVerts[8] = {Vec3(x1, y1, z1), Vec3(x2, y1, z1), Vec3(x2, y2, z1),
                             Vec3(x1, y2, z1), Vec3(x1, y1, z2), Vec3(x2, y1, z2),
                             Vec3(x2, y2, z2), Vec3(x1, y2, z2)};

        double cubeVals[8];
        int cubeIndex = 0;

        for (int i = 0; i < 8; i++)
        {
            cubeVals[i] = f(cubeVerts[i].x, cubeVerts[i].y, cubeVerts[i].z);
            if (cubeVals[i] >= 0)
                cubeIndex |= (1 << i);
        }
        flops += 8 * (EVAL_FLOPS + 1);

        if (edgeTable[cubeIndex] == 0)
            return;

        Vec3 vertList[12];
        static const int edgeConn[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                            {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                            {0, 4}, {1, 5}, {2, 6}, {3, 7}};

        for (int e = 0; e < 12; e++)
        {
            if (edgeTable[cubeIndex] & (1 << e))
            {
                int a = edgeConn[e][0], b = edgeConn[e][1];
                vertList[e] = interpolate(cubeVerts[a], cubeVerts[b],
                                          cubeVals[a], cubeVals[b]);
                flops += INTERP_FLOPS;
            }
        }

        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
        {
            for (int j = 0; j < 3; j++)
            {
                const Vec3 &v = vertList[triTable[cubeIndex][i + j]];
                vertexData.push_back((float)v.x);
                vertexData.push_back((float)v.y);
                vertexData.push_back((float)v.z);
            }
        }
    }

public:
    MarchingCubesMPI(function<double(double, double, double)> func)
        : f(func), flops(0) {}

    void addCell(double x1, double y1, double z1, double x2, double y2,
                 double z2)
    {
        processCell(x1, y1, z1, x2, y2, z2);
    }

    long long getFlops() const { return flops; }
    long long getVertexCount() const { return (long long)vertexData.size() / 3; }
    long long getTriangleCount() const { return getVertexCount() / 3; }
    const vector<float> &getVertexData() const { return vertexData; }
};

void writePLY(const string &filename, const MarchingCubesMPI &mc, int rank)
{
    long long localVerts = mc.getVertexCount();
    long long localTris = mc.getTriangleCount();
    long long vertOffset = 0, totalVerts = 0, totalTris = 0;

    MPI_Exscan(&localVerts, &vertOffset, 1, MPI_LONG_LONG, MPI_SUM,
               MPI_COMM_WORLD);
    MPI_Allreduce(&localVerts, &totalVerts, 1, MPI_LONG_LONG, MPI_SUM,
                  MPI_COMM_WORLD);
    MPI_Allreduce(&localTris, &totalTris, 1, MPI_LONG_LONG, MPI_SUM,
                  MPI_COMM_WORLD);

    ostringstream header;
    header << "ply\n"
           << "format binary_little_endian 1.0\n"
           << "element vertex " << totalVerts << "\n"
           << "property float x\n"
           << "property float y\n"
           << "property float z\n"
           << "element face " << totalTris << "\n"
           << "property list uchar int vertex_indices\n"
           << "end_header\n";
    string headerStr = header.str();
    MPI_Offset headerSize = (MPI_Offset)headerStr.size();

    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, filename.c_str(),
                  MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
    MPI_File_set_size(fh, 0);

    if (rank == 0)
        MPI_File_write_at(fh, 0, headerStr.data(), (int)headerStr.size(),
                          MPI_CHAR, MPI_STATUS_IGNORE);

    MPI_Offset vertByteOffset = headerSize + vertOffset * 3 * sizeof(float);
    MPI_File_write_at_all(fh, vertByteOffset, mc.getVertexData().data(),
                          (int)mc.getVertexData().size(), MPI_FLOAT,
                          MPI_STATUS_IGNORE);

    vector<char> faceData;
    faceData.reserve(localTris * 13);
    for (long long t = 0; t < localTris; t++)
    {
        unsigned char n = 3;
        int32_t idx[3] = {(int32_t)(vertOffset + 3 * t),
                          (int32_t)(vertOffset + 3 * t + 1),
                          (int32_t)(vertOffset + 3 * t + 2)};
        faceData.push_back((char)n);
        faceData.insert(faceData.end(), (const char *)idx,
                        (const char *)idx + sizeof(idx));
    }

    MPI_Offset triOffset = vertOffset / 3;
    MPI_Offset faceByteOffset =
        headerSize + totalVerts * 3 * sizeof(float) + triOffset * 13;
    MPI_File_write_at_all(fh, faceByteOffset, faceData.data(),
                          (int)faceData.size(), MPI_CHAR, MPI_STATUS_IGNORE);

    MPI_File_close(&fh);

    if (rank == 0)
        cout << "Archivo PLY creado: " << filename << endl;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int N = (argc > 1) ? atoi(argv[1]) : 64;
    string output = (argc > 2) ? argv[2] : "output_par.ply";

    auto torus = [](double x, double y, double z)
    {
        double R = 2.0;
        double r = 0.8;
        double dist_center = sqrt(x * x + y * y) - R;
        return r * r - (dist_center * dist_center + z * z);
    };

    double xmin = -3.5, xmax = 3.5;
    double ymin = -3.5, ymax = 3.5;
    double zmin = -1.5, zmax = 1.5;
    double dx = (xmax - xmin) / N;
    double dy = (ymax - ymin) / N;
    double dz = (zmax - zmin) / N;

    int zStart = (int)((long long)rank * N / size);
    int zEnd = (int)((long long)(rank + 1) * N / size);

    MarchingCubesMPI mc(torus);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    for (int k = zStart; k < zEnd; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++)
            {
                double x = xmin + i * dx;
                double y = ymin + j * dy;
                double z = zmin + k * dz;
                mc.addCell(x, y, z, x + dx, y + dy, z + dz);
            }

    double localCompute = MPI_Wtime() - t0;

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    writePLY(output, mc, rank);

    MPI_Barrier(MPI_COMM_WORLD);
    double t2 = MPI_Wtime();

    long long localFlops = mc.getFlops();
    long long localVerts = mc.getVertexCount();
    long long localTris = mc.getTriangleCount();
    long long totalFlops = 0, totalVerts = 0, totalTris = 0;
    double computeMax = 0, computeMin = 0;

    MPI_Reduce(&localFlops, &totalFlops, 1, MPI_LONG_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&localVerts, &totalVerts, 1, MPI_LONG_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&localTris, &totalTris, 1, MPI_LONG_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&localCompute, &computeMax, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&localCompute, &computeMin, 1, MPI_DOUBLE, MPI_MIN, 0,
               MPI_COMM_WORLD);

    if (rank == 0)
    {
        double computeTime = t1 - t0;
        double ioTime = t2 - t1;
        double totalTime = t2 - t0;
        double gflops = (totalFlops / computeTime) / 1e9;
        cout << "Procesos: " << size << endl;
        cout << "N: " << N << " (" << (long long)N * N * N << " celdas)" << endl;
        cout << "Vertices: " << totalVerts << endl;
        cout << "Triangulos: " << totalTris << endl;
        cout << "Tiempo computo: " << computeTime << " s" << endl;
        cout << "Tiempo escritura: " << ioTime << " s" << endl;
        cout << "Tiempo total: " << totalTime << " s" << endl;
        cout << "Computo por rank (max/min): " << computeMax << " / "
             << computeMin << " s" << endl;
        cout << "FLOPs: " << totalFlops << endl;
        cout << "GFLOPS: " << gflops << endl;
        cout << "CSV," << size << "," << N << "," << totalVerts << ","
             << totalTris << "," << computeTime << "," << ioTime << ","
             << totalTime << "," << computeMax << "," << computeMin << ","
             << totalFlops << "," << gflops << endl;
    }

    MPI_Finalize();
    return 0;
}
