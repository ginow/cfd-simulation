#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "PressurePoisson.hpp"
#include "KeralaClimate.hpp"

namespace cfd {

class NavierStokesSolver {
public:
    PhysicalProperties props;
    PressurePoissonSolver poisson_solver;

    NavierStokesSolver(const PhysicalProperties& p = PhysicalProperties(),
                       const SimulationConfig& cfg = SimulationConfig());

    // Advance momentum field by time step dt
    void step(Grid3D& grid, const KeralaClimate& climate, double dt);

private:
    // Advect staggered velocity components using RK2 Semi-Lagrangian / BFECC
    void advect_velocity(Grid3D& grid, double dt);

    // Apply viscous diffusion and Boussinesq thermal buoyancy force
    void apply_diffusion_and_buoyancy(Grid3D& grid, double dt);
};

} // namespace cfd
