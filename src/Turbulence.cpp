#include "Turbulence.hpp"
#include <cmath>
#include <algorithm>

namespace cfd {

void TurbulenceModel::compute_eddy_viscosity(Grid3D& grid) {
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;
    double delta = std::cbrt(grid.dx * grid.dy * grid.dz);
    double cs_delta = props.Cs * delta;
    double kappa = props.von_karman;
    double inv_dx = grid.inv_dx;
    double inv_dy = grid.inv_dy;
    double inv_dz = grid.inv_dz;

    #pragma omp parallel for schedule(static)
    for (int i = 1; i < nx - 1; ++i) {
        int iny = i * ny;
        int i_p1_ny = (i + 1) * ny;
        int i_m1_ny = (i - 1) * ny;

        for (int j = 1; j < ny - 1; ++j) {
            int base = (iny + j) * nz;
            int base_u = (iny + j) * nz;
            int base_u_p1 = (i_p1_ny + j) * nz;
            int base_u_n = (iny + j + 1) * nz;
            int base_u_s = (iny + j - 1) * nz;
            int base_u_p1_n = (i_p1_ny + j + 1) * nz;
            int base_u_p1_s = (i_p1_ny + j - 1) * nz;

            int base_v = (i * (ny + 1) + j) * nz;
            int base_v_p1 = (i * (ny + 1) + j + 1) * nz;
            int base_v_e = ((i + 1) * (ny + 1) + j) * nz;
            int base_v_w = ((i - 1) * (ny + 1) + j) * nz;
            int base_v_e_p1 = ((i + 1) * (ny + 1) + j + 1) * nz;
            int base_v_w_p1 = ((i - 1) * (ny + 1) + j + 1) * nz;

            int base_w = (iny + j) * (nz + 1);
            int base_w_n = (iny + j + 1) * (nz + 1);
            int base_w_s = (iny + j - 1) * (nz + 1);
            int base_w_e = (i_p1_ny + j) * (nz + 1);
            int base_w_w = (i_m1_ny + j) * (nz + 1);

            for (int k = 1; k < nz - 1; ++k) {
                size_t c_idx = base + k;
                if (grid.is_solid(i, j, k)) {
                    grid.nu_t[c_idx] = 0.0;
                    continue;
                }

                double sxx = (grid.u[base_u_p1 + k] - grid.u[base_u + k]) * inv_dx;
                double syy = (grid.v[base_v_p1 + k] - grid.v[base_v + k]) * inv_dy;
                double szz = (grid.w[base_w + k + 1] - grid.w[base_w + k]) * inv_dz;

                double du_dy = 0.25 * ((grid.u[base_u_n + k] + grid.u[base_u_p1_n + k]) -
                                       (grid.u[base_u_s + k] + grid.u[base_u_p1_s + k])) * inv_dy;
                double dv_dx = 0.25 * ((grid.v[base_v_e + k] + grid.v[base_v_e_p1 + k]) -
                                       (grid.v[base_v_w + k] + grid.v[base_v_w_p1 + k])) * inv_dx;
                double sxy = 0.5 * (du_dy + dv_dx);

                double dv_dz = 0.25 * ((grid.v[base_v + k + 1] + grid.v[base_v_p1 + k + 1]) -
                                       (grid.v[base_v + k - 1] + grid.v[base_v_p1 + k - 1])) * inv_dz;
                double dw_dy = 0.25 * ((grid.w[base_w_n + k] + grid.w[base_w_n + k + 1]) -
                                       (grid.w[base_w_s + k] + grid.w[base_w_s + k + 1])) * inv_dy;
                double syz = 0.5 * (dv_dz + dw_dy);

                double du_dz = 0.25 * ((grid.u[base_u + k + 1] + grid.u[base_u_p1 + k + 1]) -
                                       (grid.u[base_u + k - 1] + grid.u[base_u_p1 + k - 1])) * inv_dz;
                double dw_dx = 0.25 * ((grid.w[base_w_e + k] + grid.w[base_w_e + k + 1]) -
                                       (grid.w[base_w_w + k] + grid.w[base_w_w + k + 1])) * inv_dx;
                double sxz = 0.5 * (du_dz + dw_dx);

                double s_mag2 = 2.0 * (sxx * sxx + syy * syy + szz * szz +
                                       2.0 * (sxy * sxy + syz * syz + sxz * sxz));
                double s_mag = std::sqrt(std::max(s_mag2, 0.0));

                double yw = grid.wall_dist[c_idx];
                double ls = std::min(cs_delta, kappa * yw);

                grid.nu_t[c_idx] = ls * ls * s_mag;
            }
        }
    }
}

} // namespace cfd
