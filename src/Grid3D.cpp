#include "Grid3D.hpp"
#include <queue>
#include <limits>
#include <cmath>
#include <iostream>

namespace cfd {

Grid3D::Grid3D(int _nx, int _ny, int _nz, double _lx, double _ly, double _lz)
    : nx(_nx), ny(_ny), nz(_nz),
      lx(_lx), ly(_ly), lz(_lz),
      dx(_lx / _nx), dy(_ly / _ny), dz(_lz / _nz),
      inv_dx(_nx / _lx), inv_dy(_ny / _ly), inv_dz(_nz / _lz) {
    
    // Allocate staggered velocities
    size_t u_size = static_cast<size_t>(nx + 1) * ny * nz;
    size_t v_size = static_cast<size_t>(nx) * (ny + 1) * nz;
    size_t w_size = static_cast<size_t>(nx) * ny * (nz + 1);
    size_t cell_size = static_cast<size_t>(nx) * ny * nz;

    u.assign(u_size, 0.0);
    u_temp.assign(u_size, 0.0);

    v.assign(v_size, 0.0);
    v_temp.assign(v_size, 0.0);

    w.assign(w_size, 0.0);
    w_temp.assign(w_size, 0.0);

    T.assign(cell_size, 32.0); // Ambient 32°C initial temperature
    T_temp.assign(cell_size, 32.0);
    p.assign(cell_size, 0.0);
    div.assign(cell_size, 0.0);
    nu_t.assign(cell_size, 0.0);
    wall_dist.assign(cell_size, 1.0);
    heat_source.assign(cell_size, 0.0);
    cell_type.assign(cell_size, CELL_FLUID);
}

void Grid3D::reset() {
    std::fill(u.begin(), u.end(), 0.0);
    std::fill(u_temp.begin(), u_temp.end(), 0.0);
    std::fill(v.begin(), v.end(), 0.0);
    std::fill(v_temp.begin(), v_temp.end(), 0.0);
    std::fill(w.begin(), w.end(), 0.0);
    std::fill(w_temp.begin(), w_temp.end(), 0.0);
    std::fill(T.begin(), T.end(), 32.0);
    std::fill(T_temp.begin(), T_temp.end(), 32.0);
    std::fill(p.begin(), p.end(), 0.0);
    std::fill(div.begin(), div.end(), 0.0);
    std::fill(nu_t.begin(), nu_t.end(), 0.0);
}

Vec3 Grid3D::get_velocity_at_cell(int i, int j, int k) const {
    if (!in_bounds_cell(i, j, k)) return Vec3(0, 0, 0);
    double u_val = 0.5 * (u[idx_u(i, j, k)] + u[idx_u(i + 1, j, k)]);
    double v_val = 0.5 * (v[idx_v(i, j, k)] + v[idx_v(i, j + 1, k)]);
    double w_val = 0.5 * (w[idx_w(i, j, k)] + w[idx_w(i, j, k + 1)]);
    return Vec3(u_val, v_val, w_val);
}

double Grid3D::sample_u(double x, double y, double z) const {
    // u is located at (i * dx, (j + 0.5) * dy, (k + 0.5) * dz)
    double fi = x * inv_dx;
    double fj = y * inv_dy - 0.5;
    double fk = z * inv_dz - 0.5;

    int i0 = static_cast<int>(std::floor(fi));
    int j0 = static_cast<int>(std::floor(fj));
    int k0 = static_cast<int>(std::floor(fk));

    double fx = fi - i0;
    double fy = fj - j0;
    double fz = fk - k0;

    i0 = std::clamp(i0, 0, nx);
    int i1 = std::clamp(i0 + 1, 0, nx);
    j0 = std::clamp(j0, 0, ny - 1);
    int j1 = std::clamp(j0 + 1, 0, ny - 1);
    k0 = std::clamp(k0, 0, nz - 1);
    int k1 = std::clamp(k0 + 1, 0, nz - 1);

    double c000 = u[idx_u(i0, j0, k0)];
    double c100 = u[idx_u(i1, j0, k0)];
    double c010 = u[idx_u(i0, j1, k0)];
    double c110 = u[idx_u(i1, j1, k0)];
    double c001 = u[idx_u(i0, j0, k1)];
    double c101 = u[idx_u(i1, j0, k1)];
    double c011 = u[idx_u(i0, j1, k1)];
    double c111 = u[idx_u(i1, j1, k1)];

    double c00 = c000 * (1.0 - fx) + c100 * fx;
    double c10 = c010 * (1.0 - fx) + c110 * fx;
    double c01 = c001 * (1.0 - fx) + c101 * fx;
    double c11 = c011 * (1.0 - fx) + c111 * fx;

    double c0 = c00 * (1.0 - fy) + c10 * fy;
    double c1 = c01 * (1.0 - fy) + c11 * fy;

    return c0 * (1.0 - fz) + c1 * fz;
}

double Grid3D::sample_v(double x, double y, double z) const {
    // v is located at ((i + 0.5) * dx, j * dy, (k + 0.5) * dz)
    double fi = x * inv_dx - 0.5;
    double fj = y * inv_dy;
    double fk = z * inv_dz - 0.5;

    int i0 = static_cast<int>(std::floor(fi));
    int j0 = static_cast<int>(std::floor(fj));
    int k0 = static_cast<int>(std::floor(fk));

    double fx = fi - i0;
    double fy = fj - j0;
    double fz = fk - k0;

    i0 = std::clamp(i0, 0, nx - 1);
    int i1 = std::clamp(i0 + 1, 0, nx - 1);
    j0 = std::clamp(j0, 0, ny);
    int j1 = std::clamp(j0 + 1, 0, ny);
    k0 = std::clamp(k0, 0, nz - 1);
    int k1 = std::clamp(k0 + 1, 0, nz - 1);

    double c000 = v[idx_v(i0, j0, k0)];
    double c100 = v[idx_v(i1, j0, k0)];
    double c010 = v[idx_v(i0, j1, k0)];
    double c110 = v[idx_v(i1, j1, k0)];
    double c001 = v[idx_v(i0, j0, k1)];
    double c101 = v[idx_v(i1, j0, k1)];
    double c011 = v[idx_v(i0, j1, k1)];
    double c111 = v[idx_v(i1, j1, k1)];

    double c00 = c000 * (1.0 - fx) + c100 * fx;
    double c10 = c010 * (1.0 - fx) + c110 * fx;
    double c01 = c001 * (1.0 - fx) + c101 * fx;
    double c11 = c011 * (1.0 - fx) + c111 * fx;

    double c0 = c00 * (1.0 - fy) + c10 * fy;
    double c1 = c01 * (1.0 - fy) + c11 * fy;

    return c0 * (1.0 - fz) + c1 * fz;
}

double Grid3D::sample_w(double x, double y, double z) const {
    // w is located at ((i + 0.5) * dx, (j + 0.5) * dy, k * dz)
    double fi = x * inv_dx - 0.5;
    double fj = y * inv_dy - 0.5;
    double fk = z * inv_dz;

    int i0 = static_cast<int>(std::floor(fi));
    int j0 = static_cast<int>(std::floor(fj));
    int k0 = static_cast<int>(std::floor(fk));

    double fx = fi - i0;
    double fy = fj - j0;
    double fz = fk - k0;

    i0 = std::clamp(i0, 0, nx - 1);
    int i1 = std::clamp(i0 + 1, 0, nx - 1);
    j0 = std::clamp(j0, 0, ny - 1);
    int j1 = std::clamp(j0 + 1, 0, ny - 1);
    k0 = std::clamp(k0, 0, nz);
    int k1 = std::clamp(k0 + 1, 0, nz);

    double c000 = w[idx_w(i0, j0, k0)];
    double c100 = w[idx_w(i1, j0, k0)];
    double c010 = w[idx_w(i0, j1, k0)];
    double c110 = w[idx_w(i1, j1, k0)];
    double c001 = w[idx_w(i0, j0, k1)];
    double c101 = w[idx_w(i1, j0, k1)];
    double c011 = w[idx_w(i0, j1, k1)];
    double c111 = w[idx_w(i1, j1, k1)];

    double c00 = c000 * (1.0 - fx) + c100 * fx;
    double c10 = c010 * (1.0 - fx) + c110 * fx;
    double c01 = c001 * (1.0 - fx) + c101 * fx;
    double c11 = c011 * (1.0 - fx) + c111 * fx;

    double c0 = c00 * (1.0 - fy) + c10 * fy;
    double c1 = c01 * (1.0 - fy) + c11 * fy;

    return c0 * (1.0 - fz) + c1 * fz;
}

Vec3 Grid3D::sample_velocity(double x, double y, double z) const {
    return Vec3(sample_u(x, y, z), sample_v(x, y, z), sample_w(x, y, z));
}

double Grid3D::sample_temperature(double x, double y, double z) const {
    double fi = x * inv_dx - 0.5;
    double fj = y * inv_dy - 0.5;
    double fk = z * inv_dz - 0.5;

    int i0 = static_cast<int>(std::floor(fi));
    int j0 = static_cast<int>(std::floor(fj));
    int k0 = static_cast<int>(std::floor(fk));

    double fx = fi - i0;
    double fy = fj - j0;
    double fz = fk - k0;

    i0 = std::clamp(i0, 0, nx - 1);
    int i1 = std::clamp(i0 + 1, 0, nx - 1);
    j0 = std::clamp(j0, 0, ny - 1);
    int j1 = std::clamp(j0 + 1, 0, ny - 1);
    k0 = std::clamp(k0, 0, nz - 1);
    int k1 = std::clamp(k0 + 1, 0, nz - 1);

    double c000 = T[idx_cell(i0, j0, k0)];
    double c100 = T[idx_cell(i1, j0, k0)];
    double c010 = T[idx_cell(i0, j1, k0)];
    double c110 = T[idx_cell(i1, j1, k0)];
    double c001 = T[idx_cell(i0, j0, k1)];
    double c101 = T[idx_cell(i1, j0, k1)];
    double c011 = T[idx_cell(i0, j1, k1)];
    double c111 = T[idx_cell(i1, j1, k1)];

    double c00 = c000 * (1.0 - fx) + c100 * fx;
    double c10 = c010 * (1.0 - fx) + c110 * fx;
    double c01 = c001 * (1.0 - fx) + c101 * fx;
    double c11 = c011 * (1.0 - fx) + c111 * fx;

    double c0 = c00 * (1.0 - fy) + c10 * fy;
    double c1 = c01 * (1.0 - fy) + c11 * fy;

    return c0 * (1.0 - fz) + c1 * fz;
}

double Grid3D::sample_pressure(double x, double y, double z) const {
    double fi = x * inv_dx - 0.5;
    double fj = y * inv_dy - 0.5;
    double fk = z * inv_dz - 0.5;

    int i0 = static_cast<int>(std::floor(fi));
    int j0 = static_cast<int>(std::floor(fj));
    int k0 = static_cast<int>(std::floor(fk));

    double fx = fi - i0;
    double fy = fj - j0;
    double fz = fk - k0;

    i0 = std::clamp(i0, 0, nx - 1);
    int i1 = std::clamp(i0 + 1, 0, nx - 1);
    j0 = std::clamp(j0, 0, ny - 1);
    int j1 = std::clamp(j0 + 1, 0, ny - 1);
    k0 = std::clamp(k0, 0, nz - 1);
    int k1 = std::clamp(k0 + 1, 0, nz - 1);

    double c000 = p[idx_cell(i0, j0, k0)];
    double c100 = p[idx_cell(i1, j0, k0)];
    double c010 = p[idx_cell(i0, j1, k0)];
    double c110 = p[idx_cell(i1, j1, k0)];
    double c001 = p[idx_cell(i0, j0, k1)];
    double c101 = p[idx_cell(i1, j0, k1)];
    double c011 = p[idx_cell(i0, j1, k1)];
    double c111 = p[idx_cell(i1, j1, k1)];

    double c00 = c000 * (1.0 - fx) + c100 * fx;
    double c10 = c010 * (1.0 - fx) + c110 * fx;
    double c01 = c001 * (1.0 - fx) + c101 * fx;
    double c11 = c011 * (1.0 - fx) + c111 * fx;

    double c0 = c00 * (1.0 - fy) + c10 * fy;
    double c1 = c01 * (1.0 - fy) + c11 * fy;

    return c0 * (1.0 - fz) + c1 * fz;
}

void Grid3D::compute_wall_distances() {
    // High-performance 3D Distance Field computation using multi-pass Fast Chamfer Distance Transform (O(N) in ~1 ms)
    const double INF_DIST = 1e9;
    std::fill(wall_dist.begin(), wall_dist.end(), INF_DIST);

    // 1. Initialize boundary walls and solid cells
    #pragma omp parallel for collapse(3) schedule(static)
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            for (int k = 0; k < nz; ++k) {
                size_t c_idx = idx_cell(i, j, k);
                if (is_solid(i, j, k)) {
                    wall_dist[c_idx] = 0.0;
                } else {
                    double cx = cell_x(i);
                    double cy = cell_y(j);
                    double cz = cell_z(k);
                    // Minimum distance to domain exterior bounds
                    wall_dist[c_idx] = std::min({cx, lx - cx, cy, ly - cy, cz, lz - cz});
                }
            }
        }
    }

    // 2. Forward raster scan
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            for (int k = 0; k < nz; ++k) {
                size_t c_idx = idx_cell(i, j, k);
                double d = wall_dist[c_idx];
                if (i > 0) d = std::min(d, wall_dist[idx_cell(i - 1, j, k)] + dx);
                if (j > 0) d = std::min(d, wall_dist[idx_cell(i, j - 1, k)] + dy);
                if (k > 0) d = std::min(d, wall_dist[idx_cell(i, j, k - 1)] + dz);
                wall_dist[c_idx] = d;
            }
        }
    }

    // 3. Backward raster scan
    for (int i = nx - 1; i >= 0; --i) {
        for (int j = ny - 1; j >= 0; --j) {
            for (int k = nz - 1; k >= 0; --k) {
                size_t c_idx = idx_cell(i, j, k);
                double d = wall_dist[c_idx];
                if (i + 1 < nx) d = std::min(d, wall_dist[idx_cell(i + 1, j, k)] + dx);
                if (j + 1 < ny) d = std::min(d, wall_dist[idx_cell(i, j + 1, k)] + dy);
                if (k + 1 < nz) d = std::min(d, wall_dist[idx_cell(i, j, k + 1)] + dz);
                wall_dist[c_idx] = std::max(d, 0.01);
            }
        }
    }
}

ProbeData Grid3D::query_probe(double x, double y, double z) const {
    ProbeData data;
    data.x = std::clamp(x, 0.0, lx);
    data.y = std::clamp(y, 0.0, ly);
    data.z = std::clamp(z, 0.0, lz);

    data.i = std::clamp(static_cast<int>(data.x * inv_dx), 0, nx - 1);
    data.j = std::clamp(static_cast<int>(data.y * inv_dy), 0, ny - 1);
    data.k = std::clamp(static_cast<int>(data.z * inv_dz), 0, nz - 1);

    Vec3 vel = sample_velocity(data.x, data.y, data.z);
    data.u = vel.x;
    data.v = vel.y;
    data.w = vel.z;
    data.speed = vel.norm();

    data.temperature = sample_temperature(data.x, data.y, data.z);
    data.pressure = sample_pressure(data.x, data.y, data.z);
    
    size_t c_idx = idx_cell(data.i, data.j, data.k);
    data.eddy_viscosity = nu_t[c_idx];
    data.cell_type = cell_type[c_idx];

    return data;
}

} // namespace cfd
