#include <SDL3/SDL.h>
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

constexpr int kScreenWidth = 1000;
constexpr int kScreenHeight = 750;

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
              double camera_z, double focal_length, bool hideback = true)
{
    vector<SDL_FPoint> projected(mesh.vertices.size());
    vector<bool> visible(mesh.vertices.size(), false);

    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const Vertex3 &vertex = mesh.vertices[i];

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

    for (const auto &edge : mesh.edges)
    {
        int a = edge.first;
        int b = edge.second;

        if (visible[a] && visible[b])
        {
            SDL_RenderLine(renderer,
                           projected[a].x, projected[a].y,
                           projected[b].x, projected[b].y);
        }
    }
}

void drawFilledEarth(SDL_Renderer *renderer, const Mesh &mesh,
                     double yaw, double pitch, double camera_z, double focal_length)
{
    vector<SDL_Vertex> vertices(mesh.vertices.size());
    // convert each 3d vertex into a 2d sdl vertex
    for (size_t i = 0; i < mesh.vertices.size(); i++)
    {
        satellite point{};
        point.pos_x = mesh.vertices[i].x;
        point.pos_y = mesh.vertices[i].y;
        point.pos_z = mesh.vertices[i].z;
        applyYaw(point, yaw);
        applyPitch(point, pitch);
        double view_z = point.pos_z - camera_z;
        if (view_z <= 1.0)
            return;
        vertices[i].position = {
            static_cast<float>(kScreenWidth / 2.0 + point.pos_x / view_z * focal_length),
            static_cast<float>(kScreenHeight / 2.0 - point.pos_y / view_z * focal_length)};
        vertices[i].color = {0.05f, 0.35f, 0.85f, 1.0f};
    }
    // build the list of triangle indices
    vector<int> indices;

    for (int i = 0; i < mesh.triangles.size(); i++)
    {
        indices.push_back(mesh.triangles[i][0]);
        indices.push_back(mesh.triangles[i][1]);
        indices.push_back(mesh.triangles[i][2]);
    }

    SDL_RenderGeometry(
        renderer, nullptr,
        vertices.data(), static_cast<int>(vertices.size()),
        indices.data(), static_cast<int>(indices.size()));
}

void drawSatelliteInfo(SDL_Renderer *renderer, const satellite &s, double simulatedTime, int satelliteIndex) /// implement HUD elements
{
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

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    SDL_RenderDebugTextFormat(
        renderer, 20, 20,
        "SATELLITE %d", satelliteIndex + 1);

    SDL_RenderDebugTextFormat(
        renderer, 20, 40,
        "Altitude: %.1f km", altitudeKm);

    SDL_RenderDebugTextFormat(
        renderer, 20, 55,
        "Speed: %.2f km/s", speedKmS);
    const double GM = 3.986e14;
    /// Keplerian orbit: T = 2pi * sqrt(pow(a, 3) / GM)
    ///  calculate semi-major axis from the satellite's specific orbital energy
    if (r > 0.0)
    {
        double energy = speed * speed / 2.0 - GM / r;

        if (energy < 0.0)
        {
            double a = -GM / (2.0 * energy);
            double period = 2.0 * M_PI * sqrt(a * a * a / GM);

            SDL_RenderDebugTextFormat(
                renderer, 20, 70,
                "Orbital period: %.1f min", period / 60.0);
        }
        else
        {
            SDL_RenderDebugText(renderer, 20, 70,
                                "Orbital period: N/A");
        }
    }
    SDL_RenderDebugTextFormat(
        renderer, 20, 85,
        "Simulation time: %.1f min", simulatedTime / 60.0);
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

    SDL_Window *window = SDL_CreateWindow("Simple SDL Window", kScreenWidth, kScreenHeight, SDL_WINDOW_RESIZABLE); /// Create a window with the specified title, width, height and flags
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
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); /// Draw the stars
            SDL_RenderPoint(renderer, screen_starsx, screen_starsy);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);

        // EarthMode = 0
        if (earthMode != 0)
        {
            drawFilledEarth(renderer, earthMesh,
                            yaw, pitch, camera_z, focal_length);
        }
        // EarthMode = 1
        if (earthMode != 2)
        {
            SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);

            drawMesh(renderer, earthMesh,
                     {0.0, 0.0, 0.0}, 1.0,
                     yaw, pitch, camera_z, focal_length,
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
                     yaw, pitch, camera_z, focal_length);
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
        if (n > 0)
        {
            drawSatelliteInfo(renderer,
                              v[selectedSatellite],
                              simulatedTime,
                              selectedSatellite);
        }
        if (focusMode)
        {
            SDL_RenderDebugText(renderer, 20, 105, "VIEW: FOCUS MODE");
        }
        else
        {
            SDL_RenderDebugText(renderer, 20, 105, "VIEW: ALL SATELLITES");
        }
        SDL_RenderPresent(renderer); /// Show the render
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT)
                dragging = true;

            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event.button.button == SDL_BUTTON_LEFT)
                dragging = false;

            if (event.type == SDL_EVENT_MOUSE_MOTION && dragging)
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
            if (event.type == SDL_EVENT_MOUSE_WHEEL)
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); /// Cleans up resources by destroying the window and quitting SDL
    SDL_Quit();
    return 0;
}