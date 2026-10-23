#include "shapes.h"
#include "mesh.h"
#include <cmath>
#include <iostream>
#include <vector>
#define M_PI 3.14159265358979323846
using namespace std;
///r represents the latitude ring 
///s represents the position around that ring
///rings controls how many bands the sphere has
///segments controls how many points there are around each ring
///box is centered at (0, 0, 0), used half dimensions
Mesh makeBox(double halfW, double halfH, double halfD) {
    Mesh m;
    m.vertices = {
        {-halfW, -halfH, -halfD}, {halfW, -halfH, -halfD},
        {halfW,  halfH, -halfD}, {-halfW,  halfH, -halfD},
        {-halfW, -halfH,  halfD}, {halfW, -halfH,  halfD},
        {halfW,  halfH,  halfD}, {-halfW,  halfH,  halfD},
    }; //halfWidth, halfHeight, halfDepth
    m.edges = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
    };
    return m;
}
Mesh makeLatLongSphere(double radius, int rings, int segments) {
    Mesh m;
    ///radius controls size of sphere, rings and segments control the density
    for (int r = 0; r <= rings; r++) {
        //calculate the angle of current ring
        double phi = -M_PI / 2 + M_PI * r / (rings == 0 ? 1 : rings);
        for (int s = 0; s < segments; s++) {
            double theta = 2 * M_PI * s / segments;
            m.vertices.push_back({
                radius * cos(phi) * cos(theta),
                radius * sin(phi),
                radius * cos(phi) * sin(theta)
            });
        }
    }
    for (int r = 0; r <= rings; r++) {
        //calculate the angle around the sphere, 2*PI is one full rotation
        int base = r * segments;
        for (int s = 0; s < segments; s++)
            m.edges.push_back({base + s, base + (s + 1) % segments});
    }
    for (int r = 0; r < rings; r++) {
        //finds the starting vertex of the current ring and the next ring
        int base = r * segments, next = (r + 1) * segments;
        for (int s = 0; s < segments; s++)
        //connect each vertex to the matching vertex on the next ring
            m.edges.push_back({base + s, next + s});
    }
    for(int r = 0; r < rings; r++)
    {
        ///current stores the index of the first vertex on the current ring
        int current = r * segments;
        int next = (r+1) * segments;
        for(int s = 0; s < segments; s++)
        {
            //nextS is the following segment
            int nextS = (s+1) % segments;
            //indices of four neighbouring vertices
            int a = current + s;
            int b = next + s;
            int c = current + nextS;
            int d = next + nextS; 
            m.triangles.push_back({a, b, c});
            m.triangles.push_back({c, b, d});
        }
    }
    return m;
}
Mesh makeSatelliteModel() {
    Mesh m = makeBox(0.6, 0.3, 0.3); //the box (body) of the satellite
    auto addPanel = [&m](double xNear, double xFar) {
        int base = (int)m.vertices.size();
        //add the four corners
        m.vertices.push_back({xNear, -0.5, 0});
        m.vertices.push_back({xFar,  -0.5, 0});
        m.vertices.push_back({xFar,   0.5, 0});
        m.vertices.push_back({xNear,  0.5, 0});
        //connect the verteces with edges
        m.edges.push_back({base, base+1});
        m.edges.push_back({base+1, base+2});
        m.edges.push_back({base+2, base+3});
        m.edges.push_back({base+3, base});
    };
    addPanel(0.6, 2.0);   //add the right panel
    addPanel(-0.6, -2.0); //add the left panel
    return m;
}