#include "NavierStokesSolver.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace cfd {

NavierStokesSolver::NavierStokesSolver(const PhysicalProperties& p, const SimulationConfig& cfg)
    : props(p),
      poisson_solver(cfg.max_pressure_iters, cfg.pressure_tolerance, cfg.sor_omega) {}

void NavierStokesSolver::advect_velocity(Grid3D& grid, double dt) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;

    // 1. Advect u-component
    #pragma omp parallel for schedule(static)
    for (int i = 0; i <= nx; ++i) {
        double fx = grid.face_u_x(i);
        int iny = i * ny;
        for (int j = 0; j < ny; ++j) {
            double fy = grid.cell_y(j);
            int base_u = (iny + j) * nz;
            for (int k = 0; k < nz; ++k) {
                double fz = grid.cell_z(k);
                Vec3 vel = grid.sample_velocity(fx, fy, fz);

                double mid_x = fx - 0.5 * dt * vel.x;
                double mid_y = fy - 0.5 * dt * vel.y;
                double mid_z = fz - 0.5 * dt * vel.z;

                Vec3 mid_vel = grid.sample_velocity(mid_x, mid_y, mid_z);

                double dep_x = fx - dt * mid_vel.x;
                double dep_y = fy - dt * mid_vel.y;
                double dep_z = fz - dt * mid_vel.z;

                grid.u_temp[base_u + k] = grid.sample_u(dep_x, dep_y, dep_z);
            }
        }
    }

    // 2. Advect v-component
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        double fx = grid.cell_x(i);
        for (int j = 0; j <= ny; ++j) {
            double fy = grid.face_v_y(j);
            int base_v = (i * (ny + 1) + j) * nz;
            for (int k = 0; k < nz; ++k) {
                double fz = grid.cell_z(k);
                Vec3 vel = grid.sample_velocity(fx, fy, fz);

                double mid_x = fx - 0.5 * dt * vel.x;
                double mid_y = fy - 0.5 * dt * vel.y;
                double mid_z = fz - 0.5 * dt * vel.z;

                Vec3 mid_vel = grid.sample_velocity(mid_x, mid_y, mid_z);

                double dep_x = fx - dt * mid_vel.x;
                double dep_y = fy - dt * mid_vel.y;
                double dep_z = fz - dt * mid_vel.z;

                grid.v_temp[base_v + k] = grid.sample_v(dep_x, dep_y, dep_z);
            }
        }
    }

    // 3. Advect w-component
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < nx; ++i) {
        double fx = grid.cell_x(i);
        int iny = i * ny;
        for (int j = 0; j < ny; ++j) {
            double fy = grid.cell_y(j);
            int base_w = (iny + j) * (nz + 1);
            for (int k = 0; k <= nz; ++k) {
                double fz = grid.face_w_z(k);
                Vec3 vel = grid.sample_velocity(fx, fy, fz);

                double mid_x = fx - 0.5 * dt * vel.x;
                double mid_y = fy - 0.5 * dt * vel.y;
                double mid_z = fz - 0.5 * dt * vel.z;

                Vec3 mid_vel = grid.sample_velocity(mid_x, mid_y, mid_z);

                double dep_x = fx - dt * mid_vel.x;
                double dep_y = fy - dt * mid_vel.y;
                double dep_z = fz - dt * mid_vel.z;

                grid.w_temp[base_w + k] = grid.sample_w(dep_x, dep_y, dep_z);
            }
        }
    }

    grid.u = grid.u_temp;
    grid.v = grid.v_temp;
    grid.w = grid.w_temp;
}

void NavierStokesSolver::apply_diffusion_and_buoyancy(Grid3D& grid, double dt) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double idx2 = grid.inv_dx * grid.inv_dx;
    double idy2 = grid.inv_dy * grid.inv_dy;
    double idz2 = grid.inv_dz * grid.inv_dz;
    double beta_g = props.g * props.beta;
    double t_amb = props.T_ambient;

    // 1. Diffusion on u
    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx; ++i) {
        int iny = i * ny;
        int i_p1_ny = (i + 1) * ny;
        int i_m1_ny = (i - 1) * ny;
        for (int j = 1; j < ny - 1; ++j) {
            int base_u = (iny + j) * nz;
            int base_u_e = (i_p1_ny + j) * nz;
            int base_u_w = (i_m1_ny + j) * nz;
            int base_u_n = (iny + j + 1) * nz;
            int base_u_s = (iny + j - 1) * nz;
            int base_c = (iny + j) * nz;
            int base_c_w = (i_m1_ny + j) * nz;

            for (int k = 1; k < nz - 1; ++k) {
                if (grid.is_solid(i - 1, j, k) || grid.is_solid(i, j, k)) continue;

                double nut_avg = 0.5 * (grid.nu_t[base_c_w + k] + grid.nu_t[base_c + k]);
                double nu_eff = props.nu + nut_avg;

                size_t u_idx = base_u + k;
                double lap_u = (grid.u[base_u_e + k] - 2.0 * grid.u[u_idx] + grid.u[base_u_w + k]) * idx2 +
                               (grid.u[base_u_n + k] - 2.0 * grid.u[u_idx] + grid.u[base_u_s + k]) * idy2 +
                               (grid.u[u_idx + 1] - 2.0 * grid.u[u_idx] + grid.u[u_idx - 1]) * idz2;

                grid.u_temp[u_idx] = grid.u[u_idx] + dt * nu_eff * lap_u;
            }
        }
    }

    // 2. Diffusion & Thermal Buoyancy on v
    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx - 1; ++i) {
        int iny = i * ny;
        for (int j = 1; j < ny; ++j) {
            int base_v = (i * (ny + 1) + j) * nz;
            int base_v_e = ((i + 1) * (ny + 1) + j) * nz;
            int base_v_w = ((i - 1) * (ny + 1) + j) * nz;
            int base_v_n = (i * (ny + 1) + j + 1) * nz;
            int base_v_s = (i * (ny + 1) + j - 1) * nz;
            int base_c = (iny + j) * nz;
            int base_c_s = (iny + j - 1) * nz;

            for (int k = 1; k < nz - 1; ++k) {
                if (grid.is_solid(i, j - 1, k) || grid.is_solid(i, j, k)) continue;

                double nut_avg = 0.5 * (grid.nu_t[base_c_s + k] + grid.nu_t[base_c + k]);
                double nu_eff = props.nu + nut_avg;

                size_t v_idx = base_v + k;
                double lap_v = (grid.v[base_v_e + k] - 2.0 * grid.v[v_idx] + grid.v[base_v_w + k]) * idx2 +
                               (grid.v[base_v_n + k] - 2.0 * grid.v[v_idx] + grid.v[base_v_s + k]) * idy2 +
                               (grid.v[v_idx + 1] - 2.0 * grid.v[v_idx] + grid.v[v_idx - 1]) * idz2;

                double t_face = 0.5 * (grid.T[base_c_s + k] + grid.T[base_c + k]);
                double f_buoyancy = beta_g * (t_face - t_amb);

                grid.v_temp[v_idx] = grid.v[v_idx] + dt * (nu_eff * lap_v + f_buoyancy);
            }
        }
    }

    // 3. Diffusion on w
    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx - 1; ++i) {
        int iny = i * ny;
        int i_p1_ny = (i + 1) * ny;
        int i_m1_ny = (i - 1) * ny;
        for (int j = 1; j < ny - 1; ++j) {
            int base_w = (iny + j) * (nz + 1);
            int base_w_e = (i_p1_ny + j) * (nz + 1);
            int base_w_w = (i_m1_ny + j) * (nz + 1);
            int base_w_n = (iny + j + 1) * (nz + 1);
            int base_w_s = (iny + j - 1) * (nz + 1);
            int base_c = (iny + j) * nz;

            for (int k = 1; k < nz; ++k) {
                if (grid.is_solid(i, j, k - 1) || grid.is_solid(i, j, k)) continue;

                double nut_avg = 0.5 * (grid.nu_t[base_c + k - 1] + grid.nu_t[base_c + k]);
                double nu_eff = props.nu + nut_avg;

                size_t w_idx = base_w + k;
                double lap_w = (grid.w[base_w_e + k] - 2.0 * grid.w[w_idx] + grid.w[base_w_w + k]) * idx2 +
                               (grid.w[base_w_n + k] - 2.0 * grid.w[w_idx] + grid.w[base_w_s + k]) * idy2 +
                               (grid.w[w_idx + 1] - 2.0 * grid.w[w_idx] + grid.w[w_idx - 1]) * idz2;

                grid.w_temp[w_idx] = grid.w[w_idx] + dt * nu_eff * lap_w;
            }
        }
    }

    // Copy back
    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx; ++i) {
        int iny = i * ny;
        for (int j = 1; j < ny - 1; ++j) {
            int base_u = (iny + j) * nz;
            for (int k = 1; k < nz - 1; ++k) {
                grid.u[base_u + k] = grid.u_temp[base_u + k];
            }
        }
    }

    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx - 1; ++i) {
        for (int j = 1; j < ny; ++j) {
            int base_v = (i * (ny + 1) + j) * nz;
            for (int k = 1; k < nz - 1; ++k) {
                grid.v[base_v + k] = grid.v_temp[base_v + k];
            }
        }
    }

    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx - 1; ++i) {
        int iny = i * ny;
        for (int j = 1; j < ny - 1; ++j) {
            int base_w = (iny + j) * (nz + 1);
            for (int k = 1; k < nz; ++k) {
                grid.w[base_w + k] = grid.w_temp[base_w + k];
            }
        }
    }
}

void NavierStokesSolver::step(Grid3D& grid, [[maybe_unused]] const KeralaClimate& climate, double dt) {
    advect_velocity(grid, dt);
    apply_diffusion_and_buoyancy(grid, dt);
    poisson_solver.solve_sor(grid, dt);
    poisson_solver.project_velocity(grid, dt, props.rho_0);
}

} // namespace cfd
