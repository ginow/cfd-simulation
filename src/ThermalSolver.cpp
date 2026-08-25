#include "ThermalSolver.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace cfd {

void ThermalSolver::advect_temperature(Grid3D& grid, double dt) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        double cx = grid.cell_x(i);
        int iny = i * ny;
        for (int j = 0; j < ny; ++j) {
            double cy = grid.cell_y(j);
            int base = (iny + j) * nz;
            for (int k = 0; k < nz; ++k) {
                size_t c_idx = base + k;
                if (grid.cell_type[c_idx] == CELL_SOLID_WALL || grid.cell_type[c_idx] == CELL_HOT_BODY) {
                    grid.T_temp[c_idx] = grid.T[c_idx];
                    continue;
                }

                double cz = grid.cell_z(k);
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);

                // RK2 Midpoint backtrace
                double mid_x = cx - 0.5 * dt * vel.x;
                double mid_y = cy - 0.5 * dt * vel.y;
                double mid_z = cz - 0.5 * dt * vel.z;

                Vec3 mid_vel = grid.sample_velocity(mid_x, mid_y, mid_z);

                double dep_x = cx - dt * mid_vel.x;
                double dep_y = cy - dt * mid_vel.y;
                double dep_z = cz - dt * mid_vel.z;

                grid.T_temp[c_idx] = grid.sample_temperature(dep_x, dep_y, dep_z);
            }
        }
    }

    grid.T = grid.T_temp;
}

void ThermalSolver::diffuse_temperature(Grid3D& grid, double dt) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double idx2 = grid.inv_dx * grid.inv_dx;
    double idy2 = grid.inv_dy * grid.inv_dy;
    double idz2 = grid.inv_dz * grid.inv_dz;
    double alpha_mol = props.alpha;
    double inv_pr_t = 1.0 / props.Pr_t;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        int iny = i * ny;
        int i_p1_ny = (i + 1) * ny;
        int i_m1_ny = (i - 1) * ny;
        for (int j = 0; j < ny; ++j) {
            int base = (iny + j) * nz;
            int base_e = (i_p1_ny + j) * nz;
            int base_w = (i_m1_ny + j) * nz;
            int base_n = (iny + j + 1) * nz;
            int base_s = (iny + j - 1) * nz;

            for (int k = 0; k < nz; ++k) {
                size_t c_idx = base + k;
                if (grid.cell_type[c_idx] == CELL_SOLID_WALL || grid.cell_type[c_idx] == CELL_HOT_BODY) {
                    grid.T_temp[c_idx] = grid.T[c_idx];
                    continue;
                }

                double t_c = grid.T[c_idx];
                double alpha_eff = alpha_mol + (grid.nu_t[c_idx] * inv_pr_t);

                double t_e = (i + 1 < nx && !grid.is_solid(i + 1, j, k)) ? grid.T[base_e + k] : t_c;
                double t_w = (i - 1 >= 0 && !grid.is_solid(i - 1, j, k)) ? grid.T[base_w + k] : t_c;
                double t_n = (j + 1 < ny && !grid.is_solid(i, j + 1, k)) ? grid.T[base_n + k] : t_c;
                double t_s = (j - 1 >= 0 && !grid.is_solid(i, j - 1, k)) ? grid.T[base_s + k] : t_c;
                double t_t = (k + 1 < nz && !grid.is_solid(i, j, k + 1)) ? grid.T[base + k + 1] : t_c;
                double t_b = (k - 1 >= 0 && !grid.is_solid(i, j, k - 1)) ? grid.T[base + k - 1] : t_c;

                // Conduction with adjacent hot bodies
                if (i + 1 < nx && grid.cell_type[base_e + k] == CELL_HOT_BODY) t_e = grid.T[base_e + k];
                if (i - 1 >= 0 && grid.cell_type[base_w + k] == CELL_HOT_BODY) t_w = grid.T[base_w + k];
                if (j + 1 < ny && grid.cell_type[base_n + k] == CELL_HOT_BODY) t_n = grid.T[base_n + k];
                if (j - 1 >= 0 && grid.cell_type[base_s + k] == CELL_HOT_BODY) t_s = grid.T[base_s + k];
                if (k + 1 < nz && grid.cell_type[base + k + 1] == CELL_HOT_BODY) t_t = grid.T[base + k + 1];
                if (k - 1 >= 0 && grid.cell_type[base + k - 1] == CELL_HOT_BODY) t_b = grid.T[base + k - 1];

                double lap_t = (t_e - 2.0 * t_c + t_w) * idx2 +
                               (t_n - 2.0 * t_c + t_s) * idy2 +
                               (t_t - 2.0 * t_c + t_b) * idz2;

                grid.T_temp[c_idx] = t_c + dt * alpha_eff * lap_t;
            }
        }
    }

    grid.T = grid.T_temp;
}

void ThermalSolver::apply_thermal_sources(Grid3D& grid, [[maybe_unused]] const KeralaClimate& climate) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double t_hot = props.T_hot_body;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        int iny = i * ny;
        for (int j = 0; j < ny; ++j) {
            int base = (iny + j) * nz;
            for (int k = 0; k < nz; ++k) {
                size_t c_idx = base + k;
                if (grid.cell_type[c_idx] == CELL_HOT_BODY) {
                    grid.T[c_idx] = t_hot;
                }
            }
        }
    }
}

void ThermalSolver::step(Grid3D& grid, const KeralaClimate& climate, double dt) {
    advect_temperature(grid, dt);
    diffuse_temperature(grid, dt);
    apply_thermal_sources(grid, climate);
}

} // namespace cfd
