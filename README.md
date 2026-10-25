# Astravex — Satellite Orbit Simulator

**Astravex** is an interactive 3D satellite orbit simulator developed in **C++17**, using **SDL3** for rendering and **Dear ImGui** for its graphical interface.

The application simulates satellite motion around Earth using Newtonian gravitational physics and numerical integration. Users can create satellites, experiment with different orbital trajectories, and visualize how their orbits evolve over time.
## Preview

![Astravex Satellite Orbit Simulator](screenshots/Astravex_preview.png)

## Features

### Orbital Simulation
Simulates satellite motion under Earth's gravitational attraction.
Uses the Velocity Verlet numerical integration method.
Supports multiple satellites in the same simulation.
Visualizes satellite trajectories with colored orbital trails.
Supports circular, elliptical, polar, inclined, and escape trajectories.

### Interactive Controls
Create new simulations with custom initial conditions.
Add satellites to an existing simulation.
Choose from predefined orbit presets.
Pause, resume, and restart simulations.
Adjust simulation speed.
Rotate and zoom the 3D camera.
Switch between Earth rendering modes.
Focus on individual satellites.

### Satellite Information

View real-time orbital information, including:

Altitude and velocity
Orbital period
Perigee and apogee
Orbital eccentricity
Orbital inclination
Orbit classification

### File Management
Create a new simulation.
Open previously saved simulations.
Save and to preserve experiments.

## Getting Started

### Running the Windows Application

1. Download the Windows ZIP package from GitHub
2. Extract the ZIP file to a folder.
3. Open `Astravex.exe`.
4. Experiment with the simulation.

## How to Use Astravex

### Creating a Simulation

1. Open **File → New Simulation**.
2. Select an orbit preset or enter custom position and velocity values.
3. Adjust the simulation time step if needed.
4. Click **Create Simulation**.


## Technologies Used

- **C++17** for the application and physics implementation
- **SDL3** for rendering, window management, and input handling
- **Dear ImGui** for the GUI
- **CMake** for the build configuration

## Building from Source

The project can be built on Windows using CMake.
SDL3 and Dear ImGui are required dependencies.

From the project root, configure a Release build:

```powershell
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
```

Build the application:

```powershell
cmake --build build-release --target SatelliteOrbit
```

The executable will be generated as `Astravex.exe` in the Release build directory.

## How the Physics Works

Astravex calculates the gravitational acceleration of a satellite using Newton's law of universal gravitation:

\[
\mathbf{a} = -\frac{GM}{r^3}\mathbf{r}
\]

The simulation updates satellite motion using the **Velocity Verlet method**. More on the physics and building in an upcoming personalised documentation.

## Project Status

Astravex is under active development.