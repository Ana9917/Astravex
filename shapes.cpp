#include "shapes.h"
#include "mesh.h"
#include <cmath>
#include <iostream>
#include <vector>
#define M_PI 3.14159265358979323846
using namespace std;
/// r represents the latitude ring
/// s represents the position around that ring
/// rings controls how many bands the sphere has
/// segments controls how many points there are around each ring
/// box is centered at (0, 0, 0), used half dimensions
Mesh makeBox(double halfW, double halfH, double halfD)
{
    Mesh m;
    m.vertices = {
        {-halfW, -halfH, -halfD},
        {halfW, -halfH, -halfD},
        {halfW, halfH, -halfD},
        {-halfW, halfH, -halfD},
        {-halfW, -halfH, halfD},
        {halfW, -halfH, halfD},
        {halfW, halfH, halfD},
        {-halfW, halfH, halfD},
    }; // halfWidth, halfHeight, halfDepth
    m.edges = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    };
    return m;
}
Mesh makeLatLongSphere(double radius, int rings, int segments)
{
    Mesh m;
    /// radius controls size of sphere, rings and segments control the density
    for (int r = 0; r <= rings; r++)
    {
        // calculate the angle of current ring
        double phi = -M_PI / 2 + M_PI * r / (rings == 0 ? 1 : rings);
        for (int s = 0; s <= segments; s++)
        {
            double theta = 2 * M_PI * s / segments;
            m.vertices.push_back({radius * cos(phi) * cos(theta),
                                  radius * sin(phi),
                                  radius * cos(phi) * sin(theta)});
            float u = (float)s / segments;
            float v = 1.0f - (float)r / rings;
            m.uvs.push_back({u, v});
        }
    }
    for (int r = 0; r <= rings; r++)
    {
        // calculate the angle around the sphere, 2*PI is one full rotation
        int base = r * (segments + 1);
        for (int s = 0; s < segments; s++)
            m.edges.push_back({base + s, base + s + 1});
    }
    for (int r = 0; r < rings; r++)
    {
        // finds the starting vertex of the current ring and the next ring
        int base = r * (segments + 1);
        int next = (r + 1) * (segments + 1);
        for (int s = 0; s <= segments; s++)
            // connect each vertex to the matching vertex on the next ring
            m.edges.push_back({base + s, next + s});
    }
    for (int r = 0; r < rings; r++)
    {
        /// current stores the index of the first vertex on the current ring
        int current = r * (segments + 1);
        int next = (r + 1) * (segments + 1);
        for (int s = 0; s < segments; s++)
        {
            int a = current + s;
            int b = next + s;
            int c = current + s + 1;
            int d = next + s + 1;
            m.triangles.push_back({a, b, c});
            m.triangles.push_back({c, b, d});
        }
    }
    return m;
}
Mesh makeSatelliteModel()
{
    Mesh m = makeBox(0.6, 0.3, 0.3); // the box (body) of the satellite
    auto addPanel = [&m](double xNear, double xFar)
    {
        int base = (int)m.vertices.size();
        // add the four corners
        m.vertices.push_back({xNear, -0.5, 0});
        m.vertices.push_back({xFar, -0.5, 0});
        m.vertices.push_back({xFar, 0.5, 0});
        m.vertices.push_back({xNear, 0.5, 0});

        // connect the verteces with edges
        m.edges.push_back({base, base + 1});
        m.edges.push_back({base + 1, base + 2});
        m.edges.push_back({base + 2, base + 3});
        m.edges.push_back({base + 3, base});
        // Add three vertical divisions to the solar panel
        for (int i = 1; i <= 3; i++)
        {
            double x = xNear + (xFar - xNear) * i / 4.0;

            int bottom = (int)m.vertices.size();

            m.vertices.push_back({x, -0.5, 0});
            m.vertices.push_back({x, 0.5, 0});

            m.edges.push_back({bottom, bottom + 1});
        }
        // Add two horizontal divisions
        for (int i = 1; i <= 2; i++)
        {
            double y = -0.5 + i / 3.0;

            int left = (int)m.vertices.size();

            m.vertices.push_back({xNear, y, 0});
            m.vertices.push_back({xFar, y, 0});

            m.edges.push_back({left, left + 1});
        }
        // Create 4 columns and 3 rows of solar cells
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                double x0 = xNear + (xFar - xNear) * i / 4.0;
                double x1 = xNear + (xFar - xNear) * (i + 1) / 4.0;

                double y0 = -0.5 + j / 3.0;
                double y1 = -0.5 + (j + 1) / 3.0;

                int cell = (int)m.vertices.size();

                m.vertices.push_back({x0, y0, 0});
                m.vertices.push_back({x1, y0, 0});
                m.vertices.push_back({x1, y1, 0});
                m.vertices.push_back({x0, y1, 0});

                m.triangles.push_back({cell, cell + 1, cell + 2});
                m.triangles.push_back({cell, cell + 2, cell + 3});
            }
        }
    };
    addPanel(0.6, 2.0);   // add the right panel
    addPanel(-0.6, -2.0); // add the left panel
    // Add a small antenna above the satellite body
    int antennaBase = (int)m.vertices.size();

    // Antenna vertices
    m.vertices.push_back({0.0, 0.3, 0.0});  // Bottom
    m.vertices.push_back({0.0, 0.9, 0.0});  // Top
    m.vertices.push_back({-0.2, 1.1, 0.0}); // Left tip
    m.vertices.push_back({0.2, 1.1, 0.0});  // Right tip

    // Connect the antenna vertices
    m.edges.push_back({antennaBase, antennaBase + 1});
    m.edges.push_back({antennaBase + 1, antennaBase + 2});
    m.edges.push_back({antennaBase + 1, antennaBase + 3});
    // Front face
    m.triangles.push_back({4, 5, 6});
    m.triangles.push_back({4, 6, 7});

    // Back face
    m.triangles.push_back({0, 1, 2});
    m.triangles.push_back({0, 2, 3});

    // Left face
    m.triangles.push_back({0, 3, 7});
    m.triangles.push_back({0, 7, 4});

    // Right face
    m.triangles.push_back({1, 2, 6});
    m.triangles.push_back({1, 6, 5});

    // Top face
    m.triangles.push_back({3, 2, 6});
    m.triangles.push_back({3, 6, 7});

    // Bottom face
    m.triangles.push_back({0, 1, 5});
    m.triangles.push_back({0, 5, 4});
    return m;
}