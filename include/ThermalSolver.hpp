#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "KeralaClimate.hpp"

namespace cfd {

class ThermalSolver {
public:
    PhysicalProperties props;

    ThermalSolver(const PhysicalProperties& p = PhysicalProperties()) : props(p) {}

    // Advance temperature field by time step dt
    void step(Grid3D& grid, const KeralaClimate& climate, double dt);

private:
    // Advect temperature field with high-order BFECC / RK2 Semi-Lagrangian
    void advect_temperature(Grid3D& grid, double dt);

    // Diffuse temperature with molecular alpha + turbulent diffusivity nu_t / Pr_t
    void diffuse_temperature(Grid3D& grid, double dt);

    // Apply thermal boundary conditions & internal human metabolic heat sources
    void apply_thermal_sources(Grid3D& grid, const KeralaClimate& climate);
};

} // namespace cfd
