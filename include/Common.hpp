#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace cfd {

// Cell Type Definitions for Staggered MAC Grid
enum CellType : uint8_t {
    CELL_FLUID = 0,
    CELL_SOLID_WALL = 1,
    CELL_WINDOW_INFLOW = 2,
    CELL_WINDOW_OUTFLOW = 3,
    CELL_DOOR = 4,
    CELL_HOT_BODY = 5
};

// Physical Properties of Tropical Humid Air (Kerala, South India)
struct PhysicalProperties {
    // Ambient reference conditions
    double T_ambient = 32.0;       // °C (305.15 K)
    double T_ambient_K = 305.15;   // Kelvin
    double T_hot_body = 37.0;      // °C (310.15 K - Human core/skin temperature)
    double T_hot_body_K = 310.15;  // Kelvin

    // Thermodynamic properties of air
    double rho_0 = 1.164;          // Reference density at 32°C [kg/m^3]
    double nu = 1.615e-5;          // Kinematic viscosity [m^2/s]
    double alpha = 2.250e-5;       // Thermal diffusivity [m^2/s]
    double Cp = 1007.0;            // Specific heat capacity at constant pressure [J/(kg·K)]
    double k_air = 0.0263;         // Thermal conductivity [W/(m·K)]
    double beta = 1.0 / 305.15;    // Volumetric thermal expansion coefficient (1/T_ref) [1/K]
    double g = 9.81;               // Gravitational acceleration [m/s^2]
    double Pr = 0.71;              // Prandtl number (nu / alpha)
    double Pr_t = 0.85;            // Turbulent Prandtl number

    // LES Smagorinsky turbulence model parameters
    double Cs = 0.12;              // Smagorinsky constant
    double von_karman = 0.41;      // von Kármán constant for wall damping
};

// Kerala Climatic Wind & Weather Configuration
struct ClimateConfig {
    double wind_speed_ref = 3.5;   // Reference wind speed at 10m [m/s]
    double wind_angle_deg = 225.0; // South-West monsoon direction [degrees] (225° is SW)
    double z_ref = 10.0;           // Reference height for ABL [m]
    double terrain_alpha = 0.22;   // Power-law exponent for coastal/suburban terrain
    double outdoor_temp = 32.0;    // Ambient outdoor temperature [°C]
    double relative_humidity = 80.0;// Relative humidity [%]
    double solar_heat_flux = 150.0; // Wall solar radiation heating [W/m^2]
};

// Simulation Configuration
struct SimulationConfig {
    int nx = 64;                   // Grid resolution along X
    int ny = 28;                   // Grid resolution along Y (Height)
    int nz = 36;                   // Grid resolution along Z (Depth)

    double lx = 10.0;              // Domain length [m] (e.g. 2 rooms: 5m each)
    double ly = 3.0;               // Domain height [m] (ceiling height)
    double lz = 5.0;               // Domain width [m]

    double dt = 0.01;              // Time step [s] (adjusted dynamically by CFL)
    double max_cfl = 0.8;          // Courant-Friedrichs-Lewy stability safety limit
    int max_pressure_iters = 120;  // Maximum iterations for Poisson solver
    double pressure_tolerance = 1e-5; // Convergence tolerance for Poisson solver
    double sor_omega = 1.78;       // Successive Over-Relaxation relaxation parameter

    bool enable_les = true;        // Enable Smagorinsky LES turbulence modeling
    bool enable_buoyancy = true;   // Enable Boussinesq thermal buoyancy
};

// 3D Vector utility
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}

    double norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    double norm2() const {
        return x * x + y * y + z * z;
    }
};

// Live Inspection Probe Point
struct ProbeData {
    double x, y, z;                // Coordinate in meters
    int i, j, k;                   // Grid cell indices
    double temperature;            // °C
    double u, v, w;                // Velocity components [m/s]
    double speed;                  // Magnitude [m/s]
    double pressure;               // Relative pressure [Pa]
    double eddy_viscosity;         // SGS eddy viscosity nu_t [m^2/s]
    uint8_t cell_type;             // Type of cell
};

} // namespace cfd
