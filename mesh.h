#pragma once
#include <vector>
#include <utility>
#include <array>

using namespace std;
struct Vertex3
{
    double x, y, z;
};
struct Mesh
{
    vector<Vertex3> vertices; ///coordinates in 3D space
    vector<pair<int, int>> edges; ///vector of index pairs in vertex list
    vector<array<int, 3>> triangles; ///triangles for filling Earth
    vector<array<float, 2>> uvs; //uv coordinates to tell which part of the img belongs to which vertex
};