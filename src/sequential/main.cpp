#include "../common/cCases.h"
#include <chrono>
#include <cstdint>
#include <cstring>

class MarchingCubes
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
    MarchingCubes(function<double(double, double, double)> func)
        : f(func), flops(0) {}

    void addCell(double x1, double y1, double z1, double x2, double y2,
                 double z2)
    {
        processCell(x1, y1, z1, x2, y2, z2);
    }

    long long getFlops() const { return flops; }
    long long getVertexCount() const { return (long long)vertexData.size() / 3; }
    long long getTriangleCount() const { return getVertexCount() / 3; }

    void createPLY(const string &filename)
    {
        long long nVerts = getVertexCount();
        long long nTris = getTriangleCount();

        ofstream file(filename, ios::binary);
        file << "ply\n"
             << "format binary_little_endian 1.0\n"
             << "element vertex " << nVerts << "\n"
             << "property float x\n"
             << "property float y\n"
             << "property float z\n"
             << "element face " << nTris << "\n"
             << "property list uchar int vertex_indices\n"
             << "end_header\n";

        file.write((const char *)vertexData.data(),
                   vertexData.size() * sizeof(float));

        vector<char> faceData;
        faceData.reserve(nTris * 13);
        for (long long t = 0; t < nTris; t++)
        {
            unsigned char n = 3;
            int32_t idx[3] = {(int32_t)(3 * t), (int32_t)(3 * t + 1),
                              (int32_t)(3 * t + 2)};
            faceData.push_back((char)n);
            faceData.insert(faceData.end(), (const char *)idx,
                            (const char *)idx + sizeof(idx));
        }
        file.write(faceData.data(), faceData.size());

        file.close();
        cout << "Archivo PLY creado: " << filename << endl;
    }
};

int main(int argc, char **argv)
{
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    string output = (argc > 2) ? argv[2] : "output_seq.ply";

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

    MarchingCubes mc(torus);

    auto t0 = chrono::high_resolution_clock::now();

    for (int k = 0; k < N; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++)
            {
                double x = xmin + i * dx;
                double y = ymin + j * dy;
                double z = zmin + k * dz;
                mc.addCell(x, y, z, x + dx, y + dy, z + dz);
            }

    auto t1 = chrono::high_resolution_clock::now();
    double computeTime = chrono::duration<double>(t1 - t0).count();

    mc.createPLY(output);

    auto t2 = chrono::high_resolution_clock::now();
    double ioTime = chrono::duration<double>(t2 - t1).count();
    double totalTime = chrono::duration<double>(t2 - t0).count();

    double gflops = (mc.getFlops() / computeTime) / 1e9;
    cout << "N: " << N << " (" << (long long)N * N * N << " celdas)" << endl;
    cout << "Vertices: " << mc.getVertexCount() << endl;
    cout << "Triangulos: " << mc.getTriangleCount() << endl;
    cout << "Tiempo computo: " << computeTime << " s" << endl;
    cout << "Tiempo escritura: " << ioTime << " s" << endl;
    cout << "Tiempo total: " << totalTime << " s" << endl;
    cout << "FLOPs: " << mc.getFlops() << endl;
    cout << "GFLOPS: " << gflops << endl;
    cout << "CSV,1," << N << "," << mc.getVertexCount() << ","
         << mc.getTriangleCount() << "," << computeTime << "," << ioTime << ","
         << totalTime << "," << computeTime << "," << computeTime << ","
         << mc.getFlops() << "," << gflops << endl;

    return 0;
}
