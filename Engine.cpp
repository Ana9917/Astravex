#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <iostream>
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
                panelVertices[i].color = {0.65f, 0.50f, 0.18f, 1.0f};
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
                     double yaw, double pitch, double camera_z, double focal_length)
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
        vertices[i].color = {
            static_cast<float>(0.05 * brightness),
            static_cast<float>(0.35 * brightness),
            static_cast<float>(0.85 * brightness),
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
        renderer, nullptr,
        vertices.data(), static_cast<int>(vertices.size()),
        indices.data(), static_cast<int>(indices.size()));
}

int main()
{
    int n; /// Create a satellite
    double dt, yaw = 0.0, pitch = 0.0;
    cout << "No. of satellites ";
    cin >> n;
    cout << "Delta time ";
    cin >> dt;
    vector<deque<satellite>> trail(n);
    vector<satellite> v(n);
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
    for (int i = 0; i < n; i++)
        cin >> v[i].pos_x >> v[i].pos_y >> v[i].pos_z >> v[i].vel_x >> v[i].vel_y >> v[i].vel_z;
    double r = sqrt(v[0].pos_x * v[0].pos_x + v[0].pos_y * v[0].pos_y + v[0].pos_z * v[0].pos_z);
    for (int i = 0; i < n; i++)
        init(v[i]);
    double scale = 640.0 / 16000000.0; /// Adapt scale
    const double camera_z = -20000000.0;
    double focal_length = 400.0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) /// Check if SDL initialisation was successful and prints an error message if it was not
    {
        cout << "SDL could not initialise!" << SDL_GetError() << endl;
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
    int selectedSatellite = 0;
    bool focusMode = false; // satellite focus mode
    int earthMode = 1;      // 0 wireframe + solid, 1 solid only, 2 wireframe only
    SDL_Event event;
    constexpr double GM = 3.986e14;
    double T = 2 * M_PI * sqrt((r * r * r) / GM);          /// Calculte orbital period
    int steps = (int)(T / dt);                             /// No. of steps in one orbital period
    Mesh earthMesh = makeLatLongSphere(6371000.0, 24, 48); /// create the meshes
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
            accumulator += frameTime;

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
                            pitch, camera_z, focal_length);
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
        ImGui::SetNextWindowSize(ImVec2(340, 220), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.88f);

        ImGui::Begin("Satellite Information");

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

            ImGui::Text("Satellite %d", selectedSatellite + 1);
            ImGui::Separator();

            ImGui::Text("Altitude: %.1f km", altitudeKm);
            ImGui::Text("Velocity: %.2f km/s", speedKmS);
        }
        ImGui::End();
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
        ImGui::Text("Satellites: %d", n);
        ImGui::Text("View: %s", focusMode ? "Focus Mode" : "All Satellites");
        if (overlayfont != nullptr)
        {
            ImGui::PopFont();
        }
        ImGui::End();
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer); /// show renderer

        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
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
                earthMode = (earthMode + 1) % 3;
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

            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); /// Cleans up resources by destroying the window and quitting SDL
    SDL_Quit();
    return 0;
}