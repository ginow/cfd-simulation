#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include <vector>

namespace cfd {

class PressurePoissonSolver {
public:
    int max_iters;
    double tolerance;
    double omega; // SOR relaxation parameter

    // Conjugate Gradient work arrays
    std::vector<double> r;
    std::vector<double> d;
    std::vector<double> q;

    PressurePoissonSolver(int max_iter = 120, double tol = 1e-5, double rel_omega = 1.78);

    // Compute velocity divergence field
    void compute_divergence(Grid3D& grid, double dt);

    // Solve Poisson equation with OpenMP Red-Black SOR
    int solve_sor(Grid3D& grid, double dt);

    // Solve Poisson equation with Preconditioned Conjugate Gradient (PCG)
    int solve_pcg(Grid3D& grid, double dt);

    // Apply pressure gradient to correct intermediate velocities u*, v*, w*
    void project_velocity(Grid3D& grid, double dt, double rho_0);
};

} // namespace cfd
