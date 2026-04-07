#include "src/cCases.h"
#include <omp.h>

class MarchingCubes
{
private:
    function<double(double, double, double)> f;
    vector<tuple<double, double, double>> vertices;
    vector<tuple<int, int, int>> triangles;
    omp_lock_t lock;

    void processCell(double x1, double y1, double z1, double x2, double y2, double z2)
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

        if (edgeTable[cubeIndex] == 0)
            return;

        Vec3 vertList[12];

        if (edgeTable[cubeIndex] & 1)
            vertList[0] = interpolate(cubeVerts[0], cubeVerts[1], cubeVals[0], cubeVals[1]);
        if (edgeTable[cubeIndex] & 2)
            vertList[1] = interpolate(cubeVerts[1], cubeVerts[2], cubeVals[1], cubeVals[2]);
        if (edgeTable[cubeIndex] & 4)
            vertList[2] = interpolate(cubeVerts[2], cubeVerts[3], cubeVals[2], cubeVals[3]);
        if (edgeTable[cubeIndex] & 8)
            vertList[3] = interpolate(cubeVerts[3], cubeVerts[0], cubeVals[3], cubeVals[0]);
        if (edgeTable[cubeIndex] & 16)
            vertList[4] = interpolate(cubeVerts[4], cubeVerts[5], cubeVals[4], cubeVals[5]);
        if (edgeTable[cubeIndex] & 32)
            vertList[5] = interpolate(cubeVerts[5], cubeVerts[6], cubeVals[5], cubeVals[6]);
        if (edgeTable[cubeIndex] & 64)
            vertList[6] = interpolate(cubeVerts[6], cubeVerts[7], cubeVals[6], cubeVals[7]);
        if (edgeTable[cubeIndex] & 128)
            vertList[7] = interpolate(cubeVerts[7], cubeVerts[4], cubeVals[7], cubeVals[4]);
        if (edgeTable[cubeIndex] & 256)
            vertList[8] = interpolate(cubeVerts[0], cubeVerts[4], cubeVals[0], cubeVals[4]);
        if (edgeTable[cubeIndex] & 512)
            vertList[9] = interpolate(cubeVerts[1], cubeVerts[5], cubeVals[1], cubeVals[5]);
        if (edgeTable[cubeIndex] & 1024)
            vertList[10] = interpolate(cubeVerts[2], cubeVerts[6], cubeVals[2], cubeVals[6]);
        if (edgeTable[cubeIndex] & 2048)
            vertList[11] = interpolate(cubeVerts[3], cubeVerts[7], cubeVals[3], cubeVals[7]);

        vector<tuple<double, double, double>> localVerts;
        vector<tuple<int, int, int>> localTris;

        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
        {
            int baseIdx = localVerts.size();
            localVerts.push_back(make_tuple(vertList[triTable[cubeIndex][i]].x,
                                            vertList[triTable[cubeIndex][i]].y,
                                            vertList[triTable[cubeIndex][i]].z));
            localVerts.push_back(make_tuple(vertList[triTable[cubeIndex][i + 1]].x,
                                            vertList[triTable[cubeIndex][i + 1]].y,
                                            vertList[triTable[cubeIndex][i + 1]].z));
            localVerts.push_back(make_tuple(vertList[triTable[cubeIndex][i + 2]].x,
                                            vertList[triTable[cubeIndex][i + 2]].y,
                                            vertList[triTable[cubeIndex][i + 2]].z));
            localTris.push_back(make_tuple(baseIdx, baseIdx + 1, baseIdx + 2));
        }

        omp_set_lock(&lock);
        int baseIdx = vertices.size();
        for (auto &v : localVerts)
            vertices.push_back(v);
        for (auto &t : localTris)
            triangles.push_back(make_tuple(get<0>(t) + baseIdx,
                                           get<1>(t) + baseIdx,
                                           get<2>(t) + baseIdx));
        omp_unset_lock(&lock);
    }

public:
    MarchingCubes(function<double(double, double, double)> func) : f(func)
    {
        omp_init_lock(&lock);
    }

    ~MarchingCubes()
    {
        omp_destroy_lock(&lock);
    }

    void addCell(double x1, double y1, double z1, double x2, double y2, double z2)
    {
        processCell(x1, y1, z1, x2, y2, z2);
    }

    void createPLY(const string &filename)
    {
        ofstream file(filename);

        file << "ply" << endl;
        file << "format ascii 1.0" << endl;
        file << "element vertex " << vertices.size() << endl;
        file << "property float x" << endl;
        file << "property float y" << endl;
        file << "property float z" << endl;
        file << "element face " << triangles.size() << endl;
        file << "property list uchar int vertex_indices" << endl;
        file << "end_header" << endl;

        for (const auto &v : vertices)
            file << get<0>(v) << " " << get<1>(v) << " " << get<2>(v) << endl;

        for (const auto &t : triangles)
            file << "3 " << get<0>(t) << " " << get<1>(t) << " " << get<2>(t) << endl;

        file.close();
        cout << "Archivo PLY creado: " << filename << endl;
    }
};

void draw_surface(function<double(double, double, double)> f,
                  string output_filename, double xmin, double ymin, double zmin,
                  double xmax, double ymax, double zmax, double precision)
{
    MarchingCubes mc(f);

    vector<double> xs, ys, zs;
    for (double x = xmin; x < xmax; x += precision)
        xs.push_back(x);
    for (double y = ymin; y < ymax; y += precision)
        ys.push_back(y);
    for (double z = zmin; z < zmax; z += precision)
        zs.push_back(z);

#pragma omp parallel for collapse(3) schedule(dynamic)
    for (int i = 0; i < (int)xs.size(); i++)
        for (int j = 0; j < (int)ys.size(); j++)
            for (int k = 0; k < (int)zs.size(); k++)
                mc.addCell(xs[i], ys[j], zs[k],
                           xs[i] + precision, ys[j] + precision, zs[k] + precision);

    mc.createPLY(output_filename);
}

int main()
{
    auto torus = [](double x, double y, double z)
    {
        double R = 2.0;
        double r = 0.8;
        double dist_center = sqrt(x * x + y * y) - R;
        return r * r - (dist_center * dist_center + z * z);
    };
    cout << "Generando toro..." << endl;
    draw_surface(torus, "03_toro.ply", -3.5, -3.5, -1.5, 3.5, 3.5, 1.5, 0.1);

    return 0;
}