#include "VTKExporter.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>

namespace cfd {

bool VTKExporter::export_legacy_vtk(const Grid3D& grid, const std::string& filename, double current_time) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open VTK export file: " << filename << std::endl;
        return false;
    }

    out << "# vtk DataFile Version 3.0\n";
    out << "CFD Room Airflow & Thermal Simulation (Time: " << current_time << " s)\n";
    out << "ASCII\n";
    out << "DATASET STRUCTURED_POINTS\n";
    out << "DIMENSIONS " << grid.nx << " " << grid.ny << " " << grid.nz << "\n";
    out << "ORIGIN 0.0 0.0 0.0\n";
    out << "SPACING " << grid.dx << " " << grid.dy << " " << grid.dz << "\n";
    out << "POINT_DATA " << (grid.nx * grid.ny * grid.nz) << "\n";

    // 1. Temperature field [°C]
    out << "SCALARS Temperature double 1\n";
    out << "LOOKUP_TABLE default\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                out << std::fixed << std::setprecision(3) << grid.T[grid.idx_cell(i, j, k)] << "\n";
            }
        }
    }

    // 2. Velocity vector field [m/s]
    out << "\nVECTORS Velocity double\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                out << std::fixed << std::setprecision(4)
                    << vel.x << " " << vel.y << " " << vel.z << "\n";
            }
        }
    }

    // 3. Velocity magnitude [m/s]
    out << "\nSCALARS Velocity_Magnitude double 1\n";
    out << "LOOKUP_TABLE default\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                out << std::fixed << std::setprecision(4) << vel.norm() << "\n";
            }
        }
    }

    // 4. Pressure field [Pa]
    out << "\nSCALARS Pressure double 1\n";
    out << "LOOKUP_TABLE default\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                out << std::fixed << std::setprecision(4) << grid.p[grid.idx_cell(i, j, k)] << "\n";
            }
        }
    }

    // 5. Turbulent Eddy Viscosity [m^2/s]
    out << "\nSCALARS Eddy_Viscosity double 1\n";
    out << "LOOKUP_TABLE default\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                out << std::scientific << std::setprecision(4) << grid.nu_t[grid.idx_cell(i, j, k)] << "\n";
            }
        }
    }

    // 6. Cell Type
    out << "\nSCALARS Cell_Type int 1\n";
    out << "LOOKUP_TABLE default\n";
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                out << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]) << "\n";
            }
        }
    }

    return true;
}

} // namespace cfd
