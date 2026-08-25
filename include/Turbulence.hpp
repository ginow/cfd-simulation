#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"

namespace cfd {

class TurbulenceModel {
public:
    PhysicalProperties props;

    TurbulenceModel(const PhysicalProperties& p = PhysicalProperties()) : props(p) {}

    // Compute Subgrid-Scale (SGS) Eddy Viscosity across the 3D grid
    void compute_eddy_viscosity(Grid3D& grid);
};

} // namespace cfd
