#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include <string>

namespace cfd {

class VTKExporter {
public:
    // Export 3D structured field in legacy VTK ASCII format
    static bool export_legacy_vtk(const Grid3D& grid, const std::string& filename, double current_time = 0.0);
};

} // namespace cfd
