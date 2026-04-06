#include "src/cCases.h"

class MarchingCubes
{
private:
    function<double(double, double, double)> f;
    vector<tuple<double, double, double>> vertices;
    vector<tuple<int, int, int>> triangles;

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

        if (edgeTable[cubeIndex] == 0)
            return;

        Vec3 vertList[12];

        if (edgeTable[cubeIndex] & 1)
            vertList[0] =
                interpolate(cubeVerts[0], cubeVerts[1], cubeVals[0], cubeVals[1]);
        if (edgeTable[cubeIndex] & 2)
            vertList[1] =
                interpolate(cubeVerts[1], cubeVerts[2], cubeVals[1], cubeVals[2]);
        if (edgeTable[cubeIndex] & 4)
            vertList[2] =
                interpolate(cubeVerts[2], cubeVerts[3], cubeVals[2], cubeVals[3]);
        if (edgeTable[cubeIndex] & 8)
            vertList[3] =
                interpolate(cubeVerts[3], cubeVerts[0], cubeVals[3], cubeVals[0]);
        if (edgeTable[cubeIndex] & 16)
            vertList[4] =
                interpolate(cubeVerts[4], cubeVerts[5], cubeVals[4], cubeVals[5]);
        if (edgeTable[cubeIndex] & 32)
            vertList[5] =
                interpolate(cubeVerts[5], cubeVerts[6], cubeVals[5], cubeVals[6]);
        if (edgeTable[cubeIndex] & 64)
            vertList[6] =
                interpolate(cubeVerts[6], cubeVerts[7], cubeVals[6], cubeVals[7]);
        if (edgeTable[cubeIndex] & 128)
            vertList[7] =
                interpolate(cubeVerts[7], cubeVerts[4], cubeVals[7], cubeVals[4]);
        if (edgeTable[cubeIndex] & 256)
            vertList[8] =
                interpolate(cubeVerts[0], cubeVerts[4], cubeVals[0], cubeVals[4]);
        if (edgeTable[cubeIndex] & 512)
            vertList[9] =
                interpolate(cubeVerts[1], cubeVerts[5], cubeVals[1], cubeVals[5]);
        if (edgeTable[cubeIndex] & 1024)
            vertList[10] =
                interpolate(cubeVerts[2], cubeVerts[6], cubeVals[2], cubeVals[6]);
        if (edgeTable[cubeIndex] & 2048)
            vertList[11] =
                interpolate(cubeVerts[3], cubeVerts[7], cubeVals[3], cubeVals[7]);

        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
        {
            int baseIdx = vertices.size();

            vertices.push_back(make_tuple(vertList[triTable[cubeIndex][i]].x,
                                          vertList[triTable[cubeIndex][i]].y,
                                          vertList[triTable[cubeIndex][i]].z));
            vertices.push_back(make_tuple(vertList[triTable[cubeIndex][i + 1]].x,
                                          vertList[triTable[cubeIndex][i + 1]].y,
                                          vertList[triTable[cubeIndex][i + 1]].z));
            vertices.push_back(make_tuple(vertList[triTable[cubeIndex][i + 2]].x,
                                          vertList[triTable[cubeIndex][i + 2]].y,
                                          vertList[triTable[cubeIndex][i + 2]].z));

            triangles.push_back(make_tuple(baseIdx, baseIdx + 1, baseIdx + 2));
        }
    }

public:
    MarchingCubes(function<double(double, double, double)> func) : f(func) {}

    void addCell(double x1, double y1, double z1, double x2, double y2,
                 double z2)
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
        {
            file << get<0>(v) << " " << get<1>(v) << " " << get<2>(v) << endl;
        }

        for (const auto &t : triangles)
        {
            file << "3 " << get<0>(t) << " " << get<1>(t) << " " << get<2>(t) << endl;
        }

        file.close();
        cout << "Archivo PLY creado: " << filename << endl;
    }
};

void draw_surface(function<double(double, double, double)> f,
                  string output_filename, double xmin, double ymin, double zmin,
                  double xmax, double ymax, double zmax, double precision)
{
    MarchingCubes mc(f);

    function<void(double, double, double, double, double, double)> subdivide =
        [&](double x1, double y1, double z1, double x2, double y2, double z2)
    {
        if ((x2 - x1) < precision && (y2 - y1) < precision &&
            (z2 - z1) < precision)
        {
            mc.addCell(x1, y1, z1, x2, y2, z2);
            return;
        }

        double midx = (x1 + x2) / 2;
        double midy = (y1 + y2) / 2;
        double midz = (z1 + z2) / 2;

        subdivide(x1, y1, z1, midx, midy, midz);
        subdivide(midx, y1, z1, x2, midy, midz);
        subdivide(x1, midy, z1, midx, y2, midz);
        subdivide(midx, midy, z1, x2, y2, midz);
        subdivide(x1, y1, midz, midx, midy, z2);
        subdivide(midx, y1, midz, x2, midy, z2);
        subdivide(x1, midy, midz, midx, y2, z2);
        subdivide(midx, midy, midz, x2, y2, z2);
    };

    subdivide(xmin, ymin, zmin, xmax, ymax, zmax);
    mc.createPLY(output_filename);
}

int main()
{
    auto sphere = [](double x, double y, double z)
    {
        return 1.0 - (x * x + y * y + z * z);
    };
    // 3. TORO (DONUT)
    cout << "Generando toro..." << endl;
    auto torus = [](double x, double y, double z)
    {
        double R = 2.0; // Radio mayor
        double r = 0.8; // Radio menor
        double dist_center = sqrt(x * x + y * y) - R;
        return r * r - (dist_center * dist_center + z * z);
    };
    draw_surface(torus, "03_toro.ply", -3.5, -3.5, -1.5, 3.5, 3.5, 1.5, 0.1);

    return 0;
}