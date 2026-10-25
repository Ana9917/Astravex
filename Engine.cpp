#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <string>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <random>
#include <cmath>
#include <deque>
#include "satellite.h"
#include "stars.h"
#include "shapes.h"
#define M_PI 3.14159265358979323846
using namespace std;

int kScreenWidth = 1000;
int kScreenHeight = 750;

void applyYaw(satellite &s, double yaw) // Rotation on the y axis
{
    double old_x = s.pos_x, old_z = s.pos_z;
    s.pos_x = old_x * cos(yaw) + old_z * sin(yaw);
    s.pos_z = -old_x * sin(yaw) + old_z * cos(yaw);
}

void applyPitch(satellite &s, double pitch) // Rotation on the x axis
{
    double old_y = s.pos_y, old_z = s.pos_z;
    s.pos_y = old_y * cos(pitch) - old_z * sin(pitch);
    s.pos_z = old_y * sin(pitch) + old_z * cos(pitch);
}

Vertex3 orientSatellite(Vertex3 vertex, const satellite &s)
{
    // Direction away from Earth's centre
    double r = sqrt(s.pos_x * s.pos_x +
                    s.pos_y * s.pos_y +
                    s.pos_z * s.pos_z);

    if (r == 0.0)
        return vertex;

    double upX = s.pos_x / r;
    double upY = s.pos_y / r;
    double upZ = s.pos_z / r;

    // Direction of travel, perpendicular to the radial direction
    double radialSpeed = s.vel_x * upX +
                         s.vel_y * upY +
                         s.vel_z * upZ;

    double tx = s.vel_x - radialSpeed * upX;
    double ty = s.vel_y - radialSpeed * upY;
    double tz = s.vel_z - radialSpeed * upZ;

    double t = sqrt(tx * tx + ty * ty + tz * tz);

    if (t < 0.000001)
        return vertex;

    tx /= t;
    ty /= t;
    tz /= t;

    // Third direction, perpendicular to the first two
    double zx = ty * upZ - tz * upY;
    double zy = tz * upX - tx * upZ;
    double zz = tx * upY - ty * upX;

    return {
        vertex.x * tx + vertex.y * upX + vertex.z * zx,
        vertex.x * ty + vertex.y * upY + vertex.z * zy,
        vertex.x * tz + vertex.y * upZ + vertex.z * zz};
}
bool hiddenByEarth(const satellite &point, double camera_z)
{
    const double radius = 6371000.0;
    // dx, dy and dz describe the direction from the camera to the point
    double dx = point.pos_x;
    double dy = point.pos_y;
    double dz = point.pos_z - camera_z;

    double a = dx * dx + dy * dy + dz * dz;
    double b = 2.0 * camera_z * dz;
    double c = camera_z * camera_z - radius * radius;

    double discriminant = b * b - 4.0 * a * c;
    // discriminant tells whether the line intersects the sphere
    if (discriminant <= 0.0)
        return false;
    // t is where the first intersection happens along the line
    // calculate using the quadratic formula
    double t = (-b - sqrt(discriminant)) / (2.0 * a);

    return t > 0.0 && t < 0.999999;
}

void drawMesh(SDL_Renderer *renderer, const Mesh &mesh,
              Vertex3 position, double modelScale,
              double yaw, double pitch,
              double camera_z, double focal_length,
              bool hideback = true, bool fillPanels = false,
              const satellite *orientation = nullptr)
{
    vector<SDL_FPoint> projected(mesh.vertices.size());
    vector<bool> visible(mesh.vertices.size(), false);

    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        Vertex3 vertex = mesh.vertices[i];

        if (orientation != nullptr)
        {
            vertex = orientSatellite(vertex, *orientation);
        }

        // Convert a model vertex into a position
        satellite point{};
        point.pos_x = position.x + vertex.x * modelScale;
        point.pos_y = position.y + vertex.y * modelScale;
        point.pos_z = position.z + vertex.z * modelScale;

        // Apply the same view rotation used by orbit trails
        applyYaw(point, yaw);
        applyPitch(point, pitch);

        double view_z = point.pos_z - camera_z;
        if (view_z <= 1.0)
            continue;
        projected[i] = {
            static_cast<float>(kScreenWidth / 2.0 + point.pos_x / view_z * focal_length),
            static_cast<float>(kScreenHeight / 2.0 - point.pos_y / view_z * focal_length)};
        visible[i] = !hideback || !hiddenByEarth(point, camera_z);
    }

    if (fillPanels && !mesh.triangles.empty())
    {
        vector<SDL_Vertex> panelVertices(mesh.vertices.size());

        for (int i = 0; i < mesh.vertices.size(); i++)
        {
            panelVertices[i].position = projected[i];

            if (i < 8)
            {
                // Gold satellite body
                if (mesh.vertices[i].y > 0)
                    panelVertices[i].color = {0.85f, 0.68f, 0.25f, 1.0f};
                else
                    panelVertices[i].color = {0.40f, 0.30f, 0.10f, 1.0f};
            }
            else
            {
                // Dark-blue solar panels
                panelVertices[i].color = {0.03f, 0.12f, 0.40f, 1.0f};
            }

            panelVertices[i].tex_coord = {0.0f, 0.0f};
        }

        vector<int> panelIndices;
        for (int i = 0; i < mesh.triangles.size(); i++)
        {
            int a = mesh.triangles[i][0];
            int b = mesh.triangles[i][1];
            int c = mesh.triangles[i][2];

            if (visible[a] && visible[b] && visible[c])
            {
                panelIndices.push_back(a);
                panelIndices.push_back(b);
                panelIndices.push_back(c);
            }
        }

        if (!panelIndices.empty())
        {
            SDL_RenderGeometry(renderer, nullptr,
                               panelVertices.data(), (int)panelVertices.size(),
                               panelIndices.data(), (int)panelIndices.size());
        }
    }
    for (int i = 0; i < mesh.edges.size(); i++)
    {
        int a = mesh.edges[i].first;
        int b = mesh.edges[i].second;
        if (visible[a] && visible[b])
        {
            if (fillPanels)
            {
                int antennaStart = (int)mesh.vertices.size() - 4;

                if (a >= antennaStart && b >= antennaStart)
                {
                    // Silver antenna
                    SDL_SetRenderDrawColor(renderer, 220, 230, 240, 255);
                }
                else if (a < 8 && b < 8)
                {
                    // Light-gold body edges
                    SDL_SetRenderDrawColor(renderer, 230, 195, 120, 255);
                }
                else
                {
                    // Blue-grey solar panel grid
                    SDL_SetRenderDrawColor(renderer, 140, 175, 215, 255);
                }
            }
            SDL_RenderLine(renderer,
                           projected[a].x, projected[a].y,
                           projected[b].x, projected[b].y);
        }
    }
}

void drawFilledEarth(SDL_Renderer *renderer, const Mesh &mesh,
                     double yaw, double pitch, double camera_z, double focal_length, SDL_Texture *earthTexture)
{
    // Direction from Earth towards the Sun
    const double lightX = -0.5;
    const double lightY = 0.4;
    const double lightZ = -0.75;
    const double lightLength = sqrt(
        lightX * lightX +
        lightY * lightY +
        lightZ * lightZ);
    vector<SDL_Vertex> vertices(mesh.vertices.size());
    // store the rotated triangles so that they don't get shaded
    vector<satellite> awaypoints(mesh.vertices.size());
    // convert each 3d vertex into a 2d sdl vertex
    for (size_t i = 0; i < mesh.vertices.size(); i++)
    {
        satellite point{};
        point.pos_x = mesh.vertices[i].x;
        point.pos_y = mesh.vertices[i].y;
        point.pos_z = mesh.vertices[i].z;
        applyYaw(point, yaw);
        applyPitch(point, pitch);
        awaypoints[i] = point;
        // Surface normal: direction pointing outwards from Earth
        double nx = point.pos_x;
        double ny = point.pos_y;
        double nz = point.pos_z;

        double normalLength = sqrt(nx * nx + ny * ny + nz * nz);

        nx /= normalLength;
        ny /= normalLength;
        nz /= normalLength;

        // Dot product: how directly the surface faces the light
        double diffuse = (nx * lightX +
                          ny * lightY +
                          nz * lightZ) /
                         lightLength;

        diffuse = fmax(0.0, diffuse);

        // keep dark side slightly visible
        double brightness = 0.12 + 0.88 * diffuse;
        double view_z = point.pos_z - camera_z;
        if (view_z <= 1.0)
            return;
        vertices[i].position = {
            static_cast<float>(kScreenWidth / 2.0 + point.pos_x / view_z * focal_length),
            static_cast<float>(kScreenHeight / 2.0 - point.pos_y / view_z * focal_length)};
        // mesh.uvs[i][0] is U, and mesh.uvs[i][1] is V
        vertices[i].tex_coord = {
            mesh.uvs[i][0],
            mesh.uvs[i][1]};
        vertices[i].color = {
            (float)brightness,
            (float)brightness,
            (float)brightness,
            1.0f};
    }
    // build the list of triangle indices
    vector<int> indices;

    for (int i = 0; i < mesh.triangles.size(); i++)
    {
        int a = mesh.triangles[i][0]; // first vertex of i triangle
        int b = mesh.triangles[i][1]; // 2nd vertex of i triangle
        int c = mesh.triangles[i][2]; // 3rd vertex of i triangle
        // find centre of triangle in 3D
        double cx = (awaypoints[a].pos_x +
                     awaypoints[b].pos_x +
                     awaypoints[c].pos_x) /
                    3.0;

        double cy = (awaypoints[a].pos_y +
                     awaypoints[b].pos_y +
                     awaypoints[c].pos_y) /
                    3.0;

        double cz = (awaypoints[a].pos_z +
                     awaypoints[b].pos_z +
                     awaypoints[c].pos_z) /
                    3.0;
        // Direction from the triangle towards the camera
        double tocameraX = -cx;
        double tocameraY = -cy;
        double tocameraZ = camera_z - cz;

        // check if facing
        double facing = cx * tocameraX +
                        cy * tocameraY +
                        cz * tocameraZ;

        // Only draw triangles facing the camera
        if (facing > 0.0)
        {
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
        }
    }

    SDL_RenderGeometry(
        renderer, earthTexture,
        vertices.data(), (int)vertices.size(),
        indices.data(), (int)indices.size());
}

bool saveSim(const vector<satellite> &v, double dt, const string &filename)
{
    ofstream file(filename);

    if (!file)
        return false;

    file << v.size() << '\n';
    file << setprecision(17) << dt << '\n';

    for (int i = 0; i < v.size(); i++)
    {
        file << v[i].pos_x << " "
             << v[i].pos_y << " "
             << v[i].pos_z << " "
             << v[i].vel_x << " "
             << v[i].vel_y << " "
             << v[i].vel_z << '\n';
    }

    return static_cast<bool>(file);
}

bool loadSim(vector<satellite> &v, double &dt, const string &filename)
{
    ifstream file(filename);
    if (!file)
        return false;
    int n;
    double loadedDT;
    if (!(file >> n >> loadedDT) || n <= 0 || n > 10000 || loadedDT <= 0)
        return 0;
    vector<satellite> loaded(n);

    for (int i = 0; i < n; i++)
    {
        if (!(file >> loaded[i].pos_x >> loaded[i].pos_y >> loaded[i].pos_z >> loaded[i].vel_x >> loaded[i].vel_y >> loaded[i].vel_z))
        {
            return false;
        }
    }
    // replaces the satellite vector with the loaded satellites
    v = loaded;
    // updates the time step
    dt = loadedDT;
    return true;
}

string filename = "simulation.txt";

Uint32 openFileEvent = 0;

void SDLCALL onFileSelected(void *userdata,
                            const char *const *files,
                            int filter)
{
    if (!files || !files[0])
        return;

    SDL_Event event{};
    event.type = openFileEvent;
    event.user.data1 = SDL_strdup(files[0]);

    if (!SDL_PushEvent(&event))
        SDL_free(event.user.data1);
}

int main()
{
    double dt = 10.0, yaw = 0.0, pitch = 0.0;
    vector<satellite> v(1);
    v[0].pos_x = 6771000.0;
    v[0].vel_z = -7672.6;
    init(v[0]);
    int n = (int)v.size();
    vector<deque<satellite>> trail(n);
    vector<satellite> initialSatellites = v;
    vector<star> stars(1000);
    mt19937 gen(42); /// Number generator
    uniform_real_distribution<double> dist(-1e11, 1e11);
    for (int i = 0; i < stars.size(); i++)
    {
        /// Gives random values to x, y, and z
        stars[i].x = dist(gen);
        stars[i].y = dist(gen);
        stars[i].z = dist(gen);
    }

    double r = sqrt(v[0].pos_x * v[0].pos_x + v[0].pos_y * v[0].pos_y + v[0].pos_z * v[0].pos_z);

    double scale = 640.0 / 16000000.0; /// Adapt scale
    const double camera_z = -20000000.0;
    double focal_length = 400.0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) /// Check if SDL initialisation was successful and prints an error message if it was not
    {
        cout << "SDL could not initialise!" << SDL_GetError() << endl;
        return 1;
    }

    openFileEvent = SDL_RegisterEvents(1);

    if (openFileEvent == 0)
    {
        cout << "Could not register file event!\n";
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Astravex", kScreenWidth, kScreenHeight, SDL_WINDOW_RESIZABLE); /// Create a window with the specified title, width, height and flags
    if (!window)
    {
        cout << "The window could not be created" << SDL_GetError() << endl;
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL); /// Create renderer
    if (!renderer)
    {
        cout << "The renderer could not be created" << SDL_GetError() << endl;
        SDL_Quit();
        return 1;
    }
    SDL_Surface *earthSurface = SDL_LoadBMP("assets/earth_noClouds.0330.bmp");

    SDL_Texture *earthTexture = nullptr;

    if (earthSurface != nullptr)
    {
        earthTexture = SDL_CreateTextureFromSurface(renderer, earthSurface);
        SDL_DestroySurface(earthSurface);
    }
    else
    {
        cout << "Could not load Earth texture: " << SDL_GetError() << endl;
    }

    // Initialise Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // set theme
    ImGui::StyleColorsDark();
    // connect to SDL3
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGuiIO &io = ImGui::GetIO();
    ImFont *mainfont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 19.0f);
    ImFont *overlayfont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 16.0f);
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.WindowPadding = ImVec2(16, 14);
    style.ItemSpacing = ImVec2(10, 10);
    if (mainfont != nullptr)
    {
        io.FontDefault = mainfont;
    }
    else
    {
        cout << "Could not load Segoe UI font!" << endl;
    }
    bool running = true, paused = false;
    bool showShortcuts = true;     // for the shortcuts window
    bool showSatelliteInfo = true; // for the info window
    bool showAbout = false;        // for about window
    int selectedSatellite = 0;
    bool showNewSimulation = false;
    bool showAddSatellite = false;
    satellite addedSatellite{};
    addedSatellite.pos_z = -6771000.0;
    addedSatellite.vel_y = 7672.6;
    satellite newSatellite{};
    newSatellite.pos_x = 6771000.0;
    newSatellite.vel_z = -7672.6;
    double newDt = 10.0;
    bool focusMode = false; // satellite focus mode
    int earthMode = 2;      // 0 wireframe + solid, 1 solid only, 2 wireframe only
    SDL_Event event;
    constexpr double GM = 3.986e14;
    double T = 2 * M_PI * sqrt((r * r * r) / GM);          /// Calculte orbital period
    int steps = (int)(3 * T / dt);                         /// No. of steps in one orbital period
    Mesh earthMesh = makeLatLongSphere(6371000.0, 30, 60); /// create the meshes with 30 paralells, 60 meridians
    Mesh satelliteMesh = makeSatelliteModel();
    const SDL_Color trailColours[] = {
        {255, 80, 80, 255},  // red
        {255, 80, 200, 255}, // pink
        {80, 220, 120, 255}, // green
        {255, 200, 70, 255}, // orange
        {200, 100, 255, 255} // purple
    };
    const size_t trailColourCount = sizeof(trailColours) / sizeof(trailColours[0]);
    bool dragging = false;
    const double mouseSensitivity = 0.005; // mouse sensitivity
    double earthYaw = 0.0;
    const double physicsInterval = 1.0 / 60.0; /// 60 physics updates per real second
    double accumulator = 0.0;                  /// Stores how much real time has passed but hasn't been processed yet
    double speedMultiplier = 1.0;
    double simulatedTime = 0.0;
    Uint64 previousTime = SDL_GetTicksNS(); // SDL_GetTicksNS measures elapsed time in nanoseconds
    while (running)                         // Main loop that checks for events such as the user closing the window
    {
        // adapt to current renderer size
        SDL_GetRenderOutputSize(renderer, &kScreenWidth, &kScreenHeight);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        Uint64 currentTime = SDL_GetTicksNS();
        double frameTime = (currentTime - previousTime) / 1e9;
        previousTime = currentTime;
        if (frameTime > 0.25)
            frameTime = 0.25;
        if (!paused)
        {
            accumulator += frameTime * speedMultiplier;

            while (accumulator >= physicsInterval)
            {
                for (int i = 0; i < n; i++)
                {
                    update(v[i], dt);

                    // Save the new satellite position
                    trail[i].push_front(v[i]);

                    // Keep approximately one orbital period
                    if (trail[i].size() > steps)
                        trail[i].pop_back();
                }
                simulatedTime += dt;
                accumulator -= physicsInterval;
            }
        }
        else
        {
            accumulator = 0.0;
        }
        // Calculate Earth's rotation based on simulation time
        const double earthRotationPeriod = 86164.0;
        earthYaw = fmod(
            2.0 * M_PI * simulatedTime / earthRotationPeriod,
            2.0 * M_PI);
        const bool *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_LEFT])
            yaw += 0.02;
        if (state[SDL_SCANCODE_RIGHT])
            yaw -= 0.02;
        if (state[SDL_SCANCODE_UP])
            pitch += 0.02;
        if (state[SDL_SCANCODE_DOWN])
            pitch -= 0.02;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (int i = 0; i < stars.size(); i++)
        {
            double view_starz = stars[i].z - camera_z;
            double screen_starsx = kScreenWidth / 2.0 + (stars[i].x / view_starz) * focal_length;
            double screen_starsy = kScreenHeight / 2.0 - (stars[i].y / view_starz) * focal_length;
            int brightness = 170;
            float starSize = 2;
            if (i % 25 == 0)
            {
                brightness = 255;
                starSize = 5;
            }

            else if (i % 5 == 0)
            {
                brightness = 220;
                starSize = 3;
            }
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);

            // first loop is the horizontal position, x
            for (int x = 0; x < starSize; x++)
            {
                // second loop is the verical pos, y
                for (int y = 0; y < starSize; y++)
                {
                    SDL_RenderPoint(renderer,
                                    screen_starsx + x - starSize / 2, // keep star approx centred
                                    screen_starsy + y - starSize / 2);
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);

        // EarthMode solid+wireframe
        if (earthMode != 0)
        {
            drawFilledEarth(renderer, earthMesh, yaw + earthYaw,
                            pitch, camera_z, focal_length, earthTexture);
        }
        // EarthMode solid
        if (earthMode != 2)
        {
            SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);

            drawMesh(renderer, earthMesh,
                     {0.0, 0.0, 0.0}, 1.0,
                     yaw + earthYaw, pitch, camera_z, focal_length,
                     earthMode != 0);
        }

        for (int i = 0; i < n; i++)
        {
            if (focusMode && i != selectedSatellite)
            {
                continue;
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

            drawMesh(renderer, satelliteMesh,
                     {v[i].pos_x, v[i].pos_y, v[i].pos_z},
                     200000.0,
                     yaw, pitch, camera_z, focal_length,
                     true, true, &v[i]);
            for (size_t j = 0; j + 1 < trail[i].size(); ++j)
            {
                /// If there are more satellites than colours, reasign the pattern of colours
                const SDL_Color colour = trailColours[i % trailColourCount];
                SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, colour.a);
                satellite r1 = trail[i][j]; /// Copy trail points for display rotation
                satellite r2 = trail[i][j + 1];
                /// Perspective projection for the trail
                applyYaw(r1, yaw);
                applyYaw(r2, yaw);
                applyPitch(r1, pitch);
                applyPitch(r2, pitch);
                if (hiddenByEarth(r1, camera_z) || hiddenByEarth(r2, camera_z))
                {
                    continue;
                }
                double view_tz1 = r1.pos_z - camera_z;
                double view_tz2 = r2.pos_z - camera_z;
                double screen_tx = kScreenWidth / 2.0 + (r1.pos_x / view_tz1) * focal_length;
                double screen_ty = kScreenHeight / 2.0 - (r1.pos_y / view_tz1) * focal_length;
                double screen_tx2 = kScreenWidth / 2.0 + (r2.pos_x / view_tz2) * focal_length;
                double screen_ty2 = kScreenHeight / 2.0 - (r2.pos_y / view_tz2) * focal_length;
                SDL_RenderLine(renderer, screen_tx, screen_ty, screen_tx2, screen_ty2);
            }
        }

        ImGui::SetNextWindowPos(ImVec2(20, 120), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 340), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.88f);

        if (showSatelliteInfo)
        {
            ImGui::Begin("Satellite Information", &showSatelliteInfo);

            if (n > 0)
            {
                const satellite &s = v[selectedSatellite];

                const double earthRadius = 6371000.0;

                double r = sqrt(
                    s.pos_x * s.pos_x +
                    s.pos_y * s.pos_y +
                    s.pos_z * s.pos_z);

                double speed = sqrt(
                    s.vel_x * s.vel_x +
                    s.vel_y * s.vel_y +
                    s.vel_z * s.vel_z);

                double altitudeKm = (r - earthRadius) / 1000.0;
                double speedKmS = speed / 1000.0;

                // calculate orbital period using Kepler's 3rd law
                const double GM = 3.986e14;

                double energy = speed * speed / 2.0 - GM / r;
                double period = 0.0;

                // Angular momentum
                double hx = s.pos_y * s.vel_z - s.pos_z * s.vel_y;
                double hy = s.pos_z * s.vel_x - s.pos_x * s.vel_z;
                double hz = s.pos_x * s.vel_y - s.pos_y * s.vel_x;

                double hSquared = hx * hx + hy * hy + hz * hz;

                // Orbital inclination
                double inclination = 0.0;

                if (hSquared > 0.0)
                {
                    double h = sqrt(hSquared);

                    double cosInclination = hy / h;

                    cosInclination = fmax(-1.0, fmin(1.0, cosInclination));

                    inclination = acos(cosInclination) * 180.0 / M_PI;
                }
                // Orbital eccentricity
                double eccentricity = sqrt(
                    fmax(0.0, 1.0 + 2.0 * energy * hSquared / (GM * GM)));

                SDL_Color c = trailColours[selectedSatellite % trailColourCount];
                ImVec4 colour(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
                ImGui::TextColored(colour, "●");
                ImGui::SameLine();
                ImGui::Text("Satellite %d", selectedSatellite + 1);
                ImGui::Spacing();

                ImGui::SeparatorText("Current State");
                ImGui::Text("Altitude: %.1f km", altitudeKm);
                ImGui::Text("Velocity: %.2f km/s", speedKmS);

                ImGui::Spacing();
                ImGui::SeparatorText("Orbital Elements");

                if (energy < 0.0)
                {
                    double a = -GM / (2.0 * energy);
                    period = 2.0 * M_PI * sqrt(a * a * a / GM);

                    double perigeeKm = (a * (1.0 - eccentricity) - earthRadius) / 1000.0;
                    double apogeeKm = (a * (1.0 + eccentricity) - earthRadius) / 1000.0;

                    ImGui::Text("Orbital Period: %.1f min", period / 60.0);
                    ImGui::Text("Perigee: %.1f km", perigeeKm);
                    ImGui::Text("Apogee: %.1f km", apogeeKm);
                }
                else
                {
                    ImGui::Text("Orbital Period: N/A");
                    ImGui::Text("Perigee: N/A");
                    ImGui::Text("Apogee: N/A");
                }

                ImGui::Text("Eccentricity: %.4f", eccentricity);
                if (hSquared > 0.0)
                {
                    ImGui::Text("Inclination: %.1f deg", inclination);
                }
                else
                {
                    ImGui::Text("Inclination: N/A");
                }
                if (eccentricity < 0.01)
                {
                    ImGui::Text("Orbit: Nearly Circular");
                }
                else if (eccentricity < 1.0)
                {
                    ImGui::Text("Orbit: Elliptical");
                }
                else
                {
                    ImGui::Text("Orbit: Escape Trajectory");
                }
            }
            ImGui::End();
        }

        // Bg info
        ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoDecoration |     // remove title bar
            ImGuiWindowFlags_NoBackground |     // transparent bg
            ImGuiWindowFlags_NoMove |           // info in one positon
            ImGuiWindowFlags_NoInputs |         // lets mouse pass through the sim
            ImGuiWindowFlags_AlwaysAutoResize | // fits the overlay around the text
            ImGuiWindowFlags_NoSavedSettings;   // prevents storing the overlay's window settings

        ImGui::SetNextWindowPos(
            ImVec2(20, 20),
            ImGuiCond_Always);

        ImGui::Begin("Simulation Overlay", nullptr, overlayFlags);
        if (overlayfont != nullptr)
        {
            ImGui::PushFont(overlayfont);
        }
        ImGui::Text("SIMULATION STATUS");
        ImGui::Separator();

        ImGui::Text("Status: %s", paused ? "Paused" : "Running");
        ImGui::Text("Simulation time: %.1f min", simulatedTime / 60.0);
        ImGui::Text("Simulation speed: %.0fx", dt * speedMultiplier / physicsInterval);
        ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
        ImGui::Text("Satellites: %d", n);
        ImGui::Text("View: %s", focusMode ? "Focus Mode" : "All Satellites");
        if (overlayfont != nullptr)
        {
            ImGui::PopFont();
        }
        ImGui::End();
        if (showShortcuts)
        {
            ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);

            ImGui::Begin("Keyboard Shortcuts", &showShortcuts);

            ImGui::SeparatorText("Simulation");

            ImGui::Text("Space       Pause / Resume");
            ImGui::Text("I           Show / Hide Satellite Information");
            ImGui::Text("N / B       Next / Previous satellite");
            ImGui::Text("V           Toggle focus mode");
            ImGui::Text("G           Change Earth mode");

            ImGui::Spacing();
            ImGui::SeparatorText("Camera");

            ImGui::Text("Arrow keys  Rotate camera");
            ImGui::Text("C           Reset camera");
            ImGui::Text("Mouse drag     Rotate view");
            ImGui::Text("Mouse wheel    Zoom");

            ImGui::Spacing();
            ImGui::SeparatorText("Other");

            ImGui::Text("H           Show / Hide shortcuts");

            ImGui::End();
        }
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Simulation"))
                    showNewSimulation = true;
                if (ImGui::MenuItem("Add Satellite..."))
                    showAddSatellite = true;

                if (ImGui::MenuItem("Open Simulation..."))
                {
                    SDL_ShowOpenFileDialog(
                        onFileSelected,
                        nullptr,
                        window,
                        nullptr,
                        0,
                        nullptr,
                        false);
                }

                if (ImGui::MenuItem("Save Simulation"))
                {
                    if (saveSim(v, dt, filename))
                        cout << "Simulation saved successfully!\n";
                    else
                        cout << "Could not save simulation!\n";
                }

                ImGui::MenuItem("Save As...");
                ImGui::Separator();

                if (ImGui::MenuItem("Exit"))
                    running = false;

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Simulation"))
            {
                if (ImGui::MenuItem(paused ? "Resume" : "Pause", "Space"))
                {
                    paused = !paused;
                }
                if (ImGui::MenuItem("Restart Simulation"))
                {
                    v = initialSatellites;

                    trail.clear();
                    trail.resize(n);

                    selectedSatellite = 0;
                    simulatedTime = 0.0;
                    accumulator = 0.0;
                    earthYaw = 0.0;
                    paused = false;
                }
                if (ImGui::BeginMenu("Simulation Speed"))
                {
                    if (ImGui::MenuItem("0.5x", nullptr, speedMultiplier == 0.5))
                        speedMultiplier = 0.5;

                    if (ImGui::MenuItem("1x", nullptr, speedMultiplier == 1.0))
                        speedMultiplier = 1.0;

                    if (ImGui::MenuItem("2x", nullptr, speedMultiplier == 2.0))
                        speedMultiplier = 2.0;

                    if (ImGui::MenuItem("5x", nullptr, speedMultiplier == 5.0))
                        speedMultiplier = 5.0;

                    if (ImGui::MenuItem("10x", nullptr, speedMultiplier == 10.0))
                        speedMultiplier = 10.0;

                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Satellite Information", "I", &showSatelliteInfo);
                ImGui::MenuItem("Focus Mode", "V", &focusMode);

                ImGui::Separator();

                if (ImGui::BeginMenu("Earth Mode"))
                {
                    if (ImGui::MenuItem("Solid", nullptr, earthMode == 2))
                        earthMode = 2;

                    if (ImGui::MenuItem("Solid + Wireframe", nullptr, earthMode == 1))
                        earthMode = 1;

                    if (ImGui::MenuItem("Wireframe", nullptr, earthMode == 0))
                        earthMode = 0;

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Reset Camera", "C"))
                {
                    yaw = 0.0;
                    pitch = 0.0;
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("Keyboard Shortcuts", "H", &showShortcuts);
                ImGui::MenuItem("About Astravex", nullptr, &showAbout);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if (showAbout)
        {
            ImGui::Begin("About Astravex", &showAbout,
                         ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Astravex");
            ImGui::Text("Satellite Orbit Simulator");
            ImGui::Separator();
            ImGui::Text("Built with C++17, SDL3 and Dear ImGui.");

            ImGui::Spacing();
            ImGui::TextWrapped(
                "A 3D satellite orbit simulator that models "
                "orbital motion around Earth using Newtonian gravity.");

            ImGui::End();
        }
        if (showNewSimulation)
        {
            ImGui::Begin("New Simulation", &showNewSimulation,
                         ImGuiWindowFlags_AlwaysAutoResize);

            if (ImGui::BeginCombo("Orbit Preset", "Choose an orbit"))
            {
                if (ImGui::Selectable("Circular Orbit"))
                {
                    newSatellite = {};
                    newSatellite.pos_x = 6771000.0;
                    newSatellite.vel_z = -7672.6;
                }

                if (ImGui::Selectable("Elliptical Orbit"))
                {
                    newSatellite = {};
                    newSatellite.pos_x = 6771000.0;
                    newSatellite.vel_z = -8500.0;
                }

                if (ImGui::Selectable("Polar Orbit"))
                {
                    newSatellite = {};
                    newSatellite.pos_x = 6771000.0;
                    newSatellite.vel_y = 7672.6;
                }

                if (ImGui::Selectable("Inclined Orbit (45 deg)"))
                {
                    newSatellite = {};
                    newSatellite.pos_x = 6771000.0;
                    newSatellite.vel_y = 5425.35;
                    newSatellite.vel_z = -5425.35;
                }

                ImGui::EndCombo();
            }
            ImGui::SeparatorText("Initial Position (metres)");

            ImGui::InputDouble("X", &newSatellite.pos_x);
            ImGui::InputDouble("Y", &newSatellite.pos_y);
            ImGui::InputDouble("Z", &newSatellite.pos_z);

            ImGui::SeparatorText("Initial Velocity (m/s)");

            ImGui::InputDouble("VX", &newSatellite.vel_x);
            ImGui::InputDouble("VY", &newSatellite.vel_y);
            ImGui::InputDouble("VZ", &newSatellite.vel_z);

            ImGui::SeparatorText("Simulation Settings");

            ImGui::InputDouble("Time Step", &newDt);

            ImGui::Spacing();

            if (ImGui::Button("Create Simulation"))
            {
                double radius = sqrt(
                    newSatellite.pos_x * newSatellite.pos_x +
                    newSatellite.pos_y * newSatellite.pos_y +
                    newSatellite.pos_z * newSatellite.pos_z);

                double speed = sqrt(
                    newSatellite.vel_x * newSatellite.vel_x +
                    newSatellite.vel_y * newSatellite.vel_y +
                    newSatellite.vel_z * newSatellite.vel_z);

                if (newDt > 0.0 && isfinite(newDt) &&
                    isfinite(radius) && radius > 6371000.0 &&
                    isfinite(speed))
                {
                    v.assign(1, newSatellite);
                    init(v[0]);

                    n = 1;
                    dt = newDt;
                    initialSatellites = v;

                    trail.clear();
                    trail.resize(n);

                    selectedSatellite = 0;
                    simulatedTime = 0.0;
                    accumulator = 0.0;
                    earthYaw = 0.0;
                    paused = false;

                    double energy = speed * speed / 2.0 - GM / radius;

                    if (energy < 0.0)
                    {
                        double a = -GM / (2.0 * energy);
                        double period = 2.0 * M_PI * sqrt(a * a * a / GM);

                        steps = (int)(1.1 * period / dt) + 1;
                    }
                    else
                    {
                        steps = 10000;
                    }

                    showNewSimulation = false;
                }
            }

            ImGui::End();
        }
        if (showAddSatellite)
        {
            ImGui::Begin("Add Satellite", &showAddSatellite,
                         ImGuiWindowFlags_AlwaysAutoResize);

                // Orbit presets
                if (ImGui::BeginCombo("Orbit Preset", "Choose an orbit"))
                {
                    if (ImGui::Selectable("Circular Orbit"))
                    {
                        addedSatellite = {};
                        addedSatellite.pos_x = 6771000.0;
                        addedSatellite.vel_z = -7672.6;
                    }

                    if (ImGui::Selectable("Elliptical Orbit"))
                    {
                        addedSatellite = {};
                        addedSatellite.pos_x = 6771000.0;
                        addedSatellite.vel_z = -8500.0;
                    }

                    if (ImGui::Selectable("Polar Orbit"))
                    {
                        addedSatellite = {};
                        addedSatellite.pos_x = 6771000.0;
                        addedSatellite.vel_y = 7672.6;
                    }

                    if (ImGui::Selectable("Inclined Orbit (45 deg)"))
                    {
                        addedSatellite = {};
                        addedSatellite.pos_x = 6771000.0;
                        addedSatellite.vel_y = 5425.35;
                        addedSatellite.vel_z = -5425.35;
                    }

                    ImGui::EndCombo();
                }
            
            ImGui::SeparatorText("Initial Position (metres)");

            ImGui::InputDouble("X", &addedSatellite.pos_x);
            ImGui::InputDouble("Y", &addedSatellite.pos_y);
            ImGui::InputDouble("Z", &addedSatellite.pos_z);

            ImGui::SeparatorText("Initial Velocity (m/s)");

            ImGui::InputDouble("VX", &addedSatellite.vel_x);
            ImGui::InputDouble("VY", &addedSatellite.vel_y);
            ImGui::InputDouble("VZ", &addedSatellite.vel_z);

            ImGui::Spacing();
            if (ImGui::Button("Add Satellite"))
            {
                double radius = sqrt(
                    addedSatellite.pos_x * addedSatellite.pos_x +
                    addedSatellite.pos_y * addedSatellite.pos_y +
                    addedSatellite.pos_z * addedSatellite.pos_z);

                double speed = sqrt(
                    addedSatellite.vel_x * addedSatellite.vel_x +
                    addedSatellite.vel_y * addedSatellite.vel_y +
                    addedSatellite.vel_z * addedSatellite.vel_z);

                if (isfinite(radius) && radius > 6371000.0 &&
                    isfinite(speed))
                {
                    v.push_back(addedSatellite);
                    init(v.back());

                    trail.emplace_back();

                    n = (int)v.size();
                    selectedSatellite = n - 1;

                    initialSatellites.push_back(v.back());

                    showAddSatellite = false;
                }
            }

            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer); /// show renderer

        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == openFileEvent)
            {
                string path = static_cast<char *>(event.user.data1);
                SDL_free(event.user.data1);

                if (loadSim(v, dt, path))
                {
                    filename = path;
                    n = (int)v.size();

                    for (int i = 0; i < n; i++)
                        init(v[i]);

                    initialSatellites = v;
                    trail.clear();
                    trail.resize(n);

                    selectedSatellite = 0;
                    simulatedTime = 0.0;
                    accumulator = 0.0;
                    paused = false;

                    // Recalculate the trail length
                    double r = sqrt(v[0].pos_x * v[0].pos_x +
                                    v[0].pos_y * v[0].pos_y +
                                    v[0].pos_z * v[0].pos_z);

                    double speed = sqrt(v[0].vel_x * v[0].vel_x +
                                        v[0].vel_y * v[0].vel_y +
                                        v[0].vel_z * v[0].vel_z);

                    double energy = speed * speed / 2.0 - GM / r;

                    if (energy < 0.0)
                    {
                        double a = -GM / (2.0 * energy);
                        double T = 2.0 * M_PI * sqrt(a * a * a / GM);
                        steps = (int)(1.1 * T / dt) + 1;
                    }
                    else
                    {
                        steps = 10000;
                    }

                    cout << "Simulation loaded successfully!\n";
                }
                else
                {
                    cout << "Could not load simulation!\n";
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                if (!ImGui::GetIO().WantCaptureMouse)
                    dragging = true;
                else
                    dragging = false;
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event.button.button == SDL_BUTTON_LEFT)
                dragging = false;

            if (event.type == SDL_EVENT_MOUSE_MOTION && dragging && !ImGui::GetIO().WantCaptureMouse)
            {
                yaw += event.motion.xrel * mouseSensitivity;
                pitch += event.motion.yrel * mouseSensitivity;

                // Stop the view flipping upside down.
                const double pitchLimit = M_PI / 2.0 - 0.01;

                if (pitch > pitchLimit)
                    pitch = pitchLimit;
                if (pitch < -pitchLimit)
                    pitch = -pitchLimit;
            }

            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
                dragging = false;
            if (event.type == SDL_EVENT_MOUSE_WHEEL && !ImGui::GetIO().WantCaptureMouse)
            {
                double scroll = event.wheel.y;

                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                    scroll = -scroll;

                focal_length *= pow(1.1, scroll);

                if (focal_length < 100.0)
                    focal_length = 100.0;

                if (focal_length > 2000.0)
                    focal_length = 2000.0;
            }

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE)
                paused = !paused;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_G && !event.key.repeat)
                earthMode = (earthMode + 2) % 3;
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_C &&
                !event.key.repeat)
            {
                yaw = 0.0;
                pitch = 0.0;
            }
            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_N &&
                !event.key.repeat && n > 0)
            {
                selectedSatellite = (selectedSatellite + 1) % n;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_B &&
                !event.key.repeat && n > 0)
            {
                selectedSatellite = (selectedSatellite - 1 + n) % n;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_V &&
                !event.key.repeat)
            {
                focusMode = !focusMode;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_H &&
                !event.key.repeat)
            {
                showShortcuts = !showShortcuts;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_I &&
                !event.key.repeat)
            {
                showSatelliteInfo = !showSatelliteInfo;
            }

            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(earthTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); /// Cleans up resources by destroying the window and quitting SDL
    SDL_Quit();
    return 0;
}