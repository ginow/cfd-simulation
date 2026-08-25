#pragma once

#include "Common.hpp"
#include <vector>
#include <cmath>
#include <cassert>

namespace cfd {

class Grid3D {
public:
    int nx, ny, nz;
    double lx, ly, lz;
    double dx, dy, dz;
    double inv_dx, inv_dy, inv_dz;

    // Staggered Velocities
    // u: defined on (nx+1, ny, nz)
    std::vector<double> u;
    std::vector<double> u_temp;
    
    // v: defined on (nx, ny+1, nz)
    std::vector<double> v;
    std::vector<double> v_temp;

    // w: defined on (nx, ny, nz+1)
    std::vector<double> w;
    std::vector<double> w_temp;

    // Cell-centered scalars: (nx, ny, nz)
    std::vector<double> T;            // Temperature [°C]
    std::vector<double> T_temp;       // Temporary temperature for advection
    std::vector<double> p;            // Pressure [Pa]
    std::vector<double> div;          // Divergence [1/s]
    std::vector<double> nu_t;         // SGS Eddy Viscosity [m^2/s]
    std::vector<double> wall_dist;    // Wall normal distance [m]
    std::vector<double> heat_source;  // Volumetric heat source [W/m^3]
    std::vector<uint8_t> cell_type;   // CellType enum values

    Grid3D(int _nx = 64, int _ny = 28, int _nz = 36,
           double _lx = 10.0, double _ly = 3.0, double _lz = 5.0);

    void reset();

    // Fast 1D index mapping
    inline size_t idx_cell(int i, int j, int k) const {
        return (static_cast<size_t>(i) * ny + j) * nz + k;
    }
    inline size_t idx_u(int i, int j, int k) const {
        return (static_cast<size_t>(i) * ny + j) * nz + k;
    }
    inline size_t idx_v(int i, int j, int k) const {
        return (static_cast<size_t>(i) * (ny + 1) + j) * nz + k;
    }
    inline size_t idx_w(int i, int j, int k) const {
        return (static_cast<size_t>(i) * ny + j) * (nz + 1) + k;
    }

    // Physical coordinate of cell center
    inline double cell_x(int i) const { return (i + 0.5) * dx; }
    inline double cell_y(int j) const { return (j + 0.5) * dy; }
    inline double cell_z(int k) const { return (k + 0.5) * dz; }

    // Physical coordinate of velocity face
    inline double face_u_x(int i) const { return i * dx; }
    inline double face_v_y(int j) const { return j * dy; }
    inline double face_w_z(int k) const { return k * dz; }

    // Cell center velocity interpolation
    Vec3 get_velocity_at_cell(int i, int j, int k) const;

    // Trilinear interpolation of velocity at arbitrary spatial point (x, y, z)
    Vec3 sample_velocity(double x, double y, double z) const;
    double sample_u(double x, double y, double z) const;
    double sample_v(double x, double y, double z) const;
    double sample_w(double x, double y, double z) const;

    // Trilinear interpolation of temperature at arbitrary point (x, y, z)
    double sample_temperature(double x, double y, double z) const;

    // Trilinear interpolation of pressure at arbitrary point (x, y, z)
    double sample_pressure(double x, double y, double z) const;

    // Check bounds
    inline bool in_bounds_cell(int i, int j, int k) const {
        return (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz);
    }
    inline bool is_fluid(int i, int j, int k) const {
        if (!in_bounds_cell(i, j, k)) return false;
        return cell_type[idx_cell(i, j, k)] == CELL_FLUID;
    }
    inline bool is_solid(int i, int j, int k) const {
        if (!in_bounds_cell(i, j, k)) return true;
        uint8_t t = cell_type[idx_cell(i, j, k)];
        return (t == CELL_SOLID_WALL || t == CELL_HOT_BODY);
    }

    // Compute divergence at cell (i, j, k)
    inline double divergence(int i, int j, int k) const {
        double du = (u[idx_u(i + 1, j, k)] - u[idx_u(i, j, k)]) * inv_dx;
        double dv = (v[idx_v(i, j + 1, k)] - v[idx_v(i, j, k)]) * inv_dy;
        double dw = (w[idx_w(i, j, k + 1)] - w[idx_w(i, j, k)]) * inv_dz;
        return std::abs(du + dv + dw);
    }

    // Compute distance field to solid walls for turbulence damping
    void compute_wall_distances();

    // Live probe query at physical position (x, y, z)
    ProbeData query_probe(double x, double y, double z) const;
};

} // namespace cfd
