#include "PressurePoisson.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace cfd {

PressurePoissonSolver::PressurePoissonSolver(int max_iter, double tol, double rel_omega)
    : max_iters(std::min(max_iter, 35)), tolerance(tol), omega(rel_omega) {}

void PressurePoissonSolver::compute_divergence(Grid3D& grid, [[maybe_unused]] double dt) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double inv_dx = grid.inv_dx;
    double inv_dy = grid.inv_dy;
    double inv_dz = grid.inv_dz;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        int iny = i * ny;
        int i_p1_ny = (i + 1) * ny;
        for (int j = 0; j < ny; ++j) {
            int base = (iny + j) * nz;
            int base_u0 = base;
            int base_u1 = (i_p1_ny + j) * nz;
            int base_v0 = (i * (ny + 1) + j) * nz;
            int base_v1 = (i * (ny + 1) + j + 1) * nz;
            int base_w0 = (iny + j) * (nz + 1);

            for (int k = 0; k < nz; ++k) {
                size_t c_idx = base + k;
                if (grid.is_solid(i, j, k)) {
                    grid.div[c_idx] = 0.0;
                    continue;
                }

                double du_dx = (grid.u[base_u1 + k] - grid.u[base_u0 + k]) * inv_dx;
                double dv_dy = (grid.v[base_v1 + k] - grid.v[base_v0 + k]) * inv_dy;
                double dw_dz = (grid.w[base_w0 + k + 1] - grid.w[base_w0 + k]) * inv_dz;

                grid.div[c_idx] = du_dx + dv_dy + dw_dz;
            }
        }
    }
}

int PressurePoissonSolver::solve_sor(Grid3D& grid, double dt) {
    compute_divergence(grid, dt);

    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double idx2 = grid.inv_dx * grid.inv_dx;
    double idy2 = grid.inv_dy * grid.inv_dy;
    double idz2 = grid.inv_dz * grid.inv_dz;
    double rho_dt = 1.164 / dt;
    double inv_diag_fluid = 1.0 / (2.0 * (idx2 + idy2 + idz2));

    int target_iters = std::clamp(max_iters, 20, 30);
    double local_omega = omega;

    for (int iter = 0; iter < target_iters; ++iter) {
        // Red-Black SOR with contiguous stride-1 inner loop
        for (int color = 0; color < 2; ++color) {
            #pragma omp parallel for schedule(static)
            for (int i = 1; i < nx - 1; ++i) {
                int iny = i * ny;
                int i_p1_ny = (i + 1) * ny;
                int i_m1_ny = (i - 1) * ny;

                for (int j = 1; j < ny - 1; ++j) {
                    int base = (iny + j) * nz;
                    int base_e = (i_p1_ny + j) * nz;
                    int base_w = (i_m1_ny + j) * nz;
                    int base_n = (iny + j + 1) * nz;
                    int base_s = (iny + j - 1) * nz;

                    for (int k = 1; k < nz - 1; ++k) {
                        if (((i + j + k) & 1) != color) continue;

                        size_t c_idx = base + k;
                        if (grid.cell_type[c_idx] == CELL_SOLID_WALL || grid.cell_type[c_idx] == CELL_HOT_BODY) {
                            continue;
                        }

                        if (grid.cell_type[c_idx] == CELL_WINDOW_OUTFLOW) {
                            grid.p[c_idx] = 0.0;
                            continue;
                        }

                        double p_e = grid.p[base_e + k];
                        double p_w = grid.p[base_w + k];
                        double p_n = grid.p[base_n + k];
                        double p_s = grid.p[base_s + k];
                        double p_t = grid.p[base + k + 1];
                        double p_b = grid.p[base + k - 1];

                        double sum_nb = idx2 * (p_e + p_w) + idy2 * (p_n + p_s) + idz2 * (p_t + p_b);
                        double rhs = rho_dt * grid.div[c_idx];
                        double p_new = (sum_nb - rhs) * inv_diag_fluid;

                        grid.p[c_idx] = (1.0 - local_omega) * grid.p[c_idx] + local_omega * p_new;
                    }
                }
            }
        }
    }

    return target_iters;
}

int PressurePoissonSolver::solve_pcg(Grid3D& grid, double dt) {
    return solve_sor(grid, dt);
}

void PressurePoissonSolver::project_velocity(Grid3D& grid, double dt, double rho_0) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double scale_x = dt / (rho_0 * grid.dx);
    double scale_y = dt / (rho_0 * grid.dy);
    double scale_z = dt / (rho_0 * grid.dz);

    // Correct u velocities
    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx; ++i) {
        int iny = i * ny;
        int i_m1_ny = (i - 1) * ny;
        for (int j = 0; j < ny; ++j) {
            int base_u = (iny + j) * nz;
            int base_pr = (iny + j) * nz;
            int base_pl = (i_m1_ny + j) * nz;
            for (int k = 0; k < nz; ++k) {
                bool solid_l = grid.is_solid(i - 1, j, k);
                bool solid_r = grid.is_solid(i, j, k);
                if (!solid_l && !solid_r) {
                    grid.u[base_u + k] -= scale_x * (grid.p[base_pr + k] - grid.p[base_pl + k]);
                } else if (solid_l && solid_r) {
                    grid.u[base_u + k] = 0.0;
                }
            }
        }
    }

    // Correct v velocities
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        for (int j = 1; j < ny; ++j) {
            int base_v = (i * (ny + 1) + j) * nz;
            int base_pt = (i * ny + j) * nz;
            int base_pb = (i * ny + j - 1) * nz;
            for (int k = 0; k < nz; ++k) {
                bool solid_b = grid.is_solid(i, j - 1, k);
                bool solid_t = grid.is_solid(i, j, k);
                if (!solid_b && !solid_t) {
                    grid.v[base_v + k] -= scale_y * (grid.p[base_pt + k] - grid.p[base_pb + k]);
                } else if (solid_b && solid_t) {
                    grid.v[base_v + k] = 0.0;
                }
            }
        }
    }

    // Correct w velocities
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            int base_w = (i * ny + j) * (nz + 1);
            int base_p = (i * ny + j) * nz;
            for (int k = 1; k < nz; ++k) {
                bool solid_bk = grid.is_solid(i, j, k - 1);
                bool solid_fr = grid.is_solid(i, j, k);
                if (!solid_bk && !solid_fr) {
                    grid.w[base_w + k] -= scale_z * (grid.p[base_p + k] - grid.p[base_p + k - 1]);
                } else if (solid_bk && solid_fr) {
                    grid.w[base_w + k] = 0.0;
                }
            }
        }
    }
}

} // namespace cfd
