#include "Geometry.hpp"
#include <cmath>
#include <iostream>

namespace cfd {

BuildingGeometry::BuildingGeometry(double lx, double ly, double lz) {
    // 1. Define Rooms
    Room r1;
    r1.id = 1;
    r1.name = "Room 1 (West Living Suite)";
    r1.bounds = {0.0, lx * 0.5, 0.0, ly, 0.0, lz};
    rooms.push_back(r1);

    Room r2;
    r2.id = 2;
    r2.name = "Room 2 (East Study Suite)";
    r2.bounds = {lx * 0.5, lx, 0.0, ly, 0.0, lz};
    rooms.push_back(r2);

    // 2. Define Windows (Two windows per room = 4 windows total)
    // Room 1 - Window 1A (West Windward Inflow)
    Window w1a;
    w1a.name = "Room 1: West Inflow Window";
    w1a.room_id = 1;
    w1a.box = {0.0, 0.25, 0.9, 2.2, 1.5, 3.5};
    w1a.normal = Vec3(1.0, 0.0, 0.0);
    w1a.open_ratio = 1.0;
    w1a.is_inflow = true;
    windows.push_back(w1a);

    // Room 1 - Window 1B (South Cross-Ventilation)
    Window w1b;
    w1b.name = "Room 1: South Window";
    w1b.room_id = 1;
    w1b.box = {1.5, 3.5, 0.9, 2.2, 0.0, 0.25};
    w1b.normal = Vec3(0.0, 0.0, 1.0);
    w1b.open_ratio = 1.0;
    w1b.is_inflow = true;
    windows.push_back(w1b);

    // Room 2 - Window 2A (North Window)
    Window w2a;
    w2a.name = "Room 2: North Window";
    w2a.room_id = 2;
    w2a.box = {6.5, 8.5, 0.9, 2.2, lz - 0.25, lz};
    w2a.normal = Vec3(0.0, 0.0, -1.0);
    w2a.open_ratio = 1.0;
    w2a.is_inflow = true;
    windows.push_back(w2a);

    // Room 2 - Window 2B (East Leeward Outflow)
    Window w2b;
    w2b.name = "Room 2: East Outflow Window";
    w2b.room_id = 2;
    w2b.box = {lx - 0.25, lx, 0.9, 2.2, 1.5, 3.5};
    w2b.normal = Vec3(1.0, 0.0, 0.0);
    w2b.open_ratio = 1.0;
    w2b.is_inflow = false;
    windows.push_back(w2b);

    // 3. Define Doors (One door per room)
    // Room 1 / Partition Door
    Door d1;
    d1.name = "Inter-Room Partition Door";
    d1.room_id = 1;
    d1.box = {lx * 0.5 - 0.15, lx * 0.5 + 0.15, 0.0, 2.1, 2.0, 3.0};
    d1.open_ratio = 1.0;
    d1.is_connecting = true;
    doors.push_back(d1);

    // Room 2 Exterior Exit Door
    Door d2;
    d2.name = "Room 2: Exterior South Door";
    d2.room_id = 2;
    d2.box = {6.5, 7.5, 0.0, 2.1, 0.0, 0.25};
    d2.open_ratio = 0.5;
    d2.is_connecting = false;
    doors.push_back(d2);

    // 4. Define Hot Bodies at 37 °C (Human Occupants)
    // Room 1 Human Occupant
    HumanBody h1;
    h1.name = "Room 1 Occupant (Human Body @ 37°C)";
    h1.room_id = 1;
    h1.box = {2.3, 2.7, 0.0, 1.7, 2.3, 2.7};
    h1.temperature = 37.0; // 37 °C
    h1.metabolic_rate = 90.0;
    occupants.push_back(h1);

    // Room 2 Human Occupant
    HumanBody h2;
    h2.name = "Room 2 Occupant (Human Body @ 37°C)";
    h2.room_id = 2;
    h2.box = {7.3, 7.7, 0.0, 1.7, 2.3, 2.7};
    h2.temperature = 37.0; // 37 °C
    h2.metabolic_rate = 90.0;
    occupants.push_back(h2);
}

void BuildingGeometry::rasterize_to_grid(Grid3D& grid, [[maybe_unused]] const KeralaClimate& climate) {
    double mid_x = grid.lx * 0.5;

    for (int i = 0; i < grid.nx; ++i) {
        double x = grid.cell_x(i);
        bool at_x_boundary = (i == 0 || i == grid.nx - 1);
        bool at_mid_wall = std::abs(x - mid_x) < (grid.dx * 0.9);

        for (int j = 0; j < grid.ny; ++j) {
            double y = grid.cell_y(j);
            bool at_y_boundary = (j == 0 || j == grid.ny - 1);

            for (int k = 0; k < grid.nz; ++k) {
                double z = grid.cell_z(k);
                bool at_z_boundary = (k == 0 || k == grid.nz - 1);

                size_t c_idx = grid.idx_cell(i, j, k);
                grid.cell_type[c_idx] = CELL_FLUID;
                grid.heat_source[c_idx] = 0.0;

                // 1. Exterior Walls and Interior Partition Wall
                if (at_x_boundary || at_y_boundary || at_z_boundary || at_mid_wall) {
                    grid.cell_type[c_idx] = CELL_SOLID_WALL;
                }

                // 2. Overlay Windows
                for (const auto& w : windows) {
                    if (w.open_ratio > 0.05 && w.box.contains(x, y, z)) {
                        grid.cell_type[c_idx] = w.is_inflow ? CELL_WINDOW_INFLOW : CELL_WINDOW_OUTFLOW;
                    }
                }

                // 3. Overlay Doors
                for (const auto& d : doors) {
                    if (d.open_ratio > 0.05 && d.box.contains(x, y, z)) {
                        grid.cell_type[c_idx] = CELL_DOOR;
                    }
                }

                // 4. Overlay Human Hot Bodies (37 °C)
                for (const auto& h : occupants) {
                    if (h.box.contains(x, y, z)) {
                        grid.cell_type[c_idx] = CELL_HOT_BODY;
                        grid.T[c_idx] = h.temperature;
                        // Volumetric heat release ~ 90W / Volume
                        double vol = (h.box.x_max - h.box.x_min) * (h.box.y_max - h.box.y_min) * (h.box.z_max - h.box.z_min);
                        grid.heat_source[c_idx] = h.metabolic_rate / std::max(vol, 0.05);
                    }
                }
            }
        }
    }

    grid.compute_wall_distances();
}

void BuildingGeometry::enforce_boundary_conditions(Grid3D& grid, const KeralaClimate& climate, double time) {
    // 1. Enforce No-Slip Velocity on Solid Walls
    #pragma omp parallel for collapse(3) schedule(static)
    for (int i = 0; i <= grid.nx; ++i) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                bool solid_left = (i == 0) || grid.is_solid(i - 1, j, k);
                bool solid_right = (i == grid.nx) || grid.is_solid(i, j, k);
                if (solid_left && solid_right) {
                    grid.u[grid.idx_u(i, j, k)] = 0.0;
                }
            }
        }
    }

    #pragma omp parallel for collapse(3) schedule(static)
    for (int i = 0; i < grid.nx; ++i) {
        for (int j = 0; j <= grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                bool solid_bottom = (j == 0) || grid.is_solid(i, j - 1, k);
                bool solid_top = (j == grid.ny) || grid.is_solid(i, j, k);
                if (solid_bottom && solid_top) {
                    grid.v[grid.idx_v(i, j, k)] = 0.0;
                }
            }
        }
    }

    #pragma omp parallel for collapse(3) schedule(static)
    for (int i = 0; i < grid.nx; ++i) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k <= grid.nz; ++k) {
                bool solid_back = (k == 0) || grid.is_solid(i, j, k - 1);
                bool solid_front = (k == grid.nz) || grid.is_solid(i, j, k);
                if (solid_back && solid_front) {
                    grid.w[grid.idx_w(i, j, k)] = 0.0;
                }
            }
        }
    }

    // 2. Inflow / Outflow Windows Boundary Conditions
    // Small wind gust perturbation for realism: sin(0.5 * time)
    double gust_factor = 1.0 + 0.12 * std::sin(0.4 * time) + 0.08 * std::cos(1.1 * time);

    for (const auto& w : windows) {
        if (w.open_ratio <= 0.01) continue;

        if (w.is_inflow) {
            // Prescribe Kerala Atmospheric Boundary Layer profile
            for (int i = 0; i < grid.nx; ++i) {
                double x = grid.cell_x(i);
                for (int j = 0; j < grid.ny; ++j) {
                    double y = grid.cell_y(j);
                    for (int k = 0; k < grid.nz; ++k) {
                        double z = grid.cell_z(k);
                        if (w.box.contains(x, y, z)) {
                            Vec3 inflow_vel = climate.get_inflow_velocity(y);
                            double effective_speed = inflow_vel.norm() * w.open_ratio * gust_factor;
                            double outdoor_t = climate.get_outdoor_temperature(y);

                            size_t c_idx = grid.idx_cell(i, j, k);
                            grid.T[c_idx] = outdoor_t;

                            if (w.normal.x > 0.5) {
                                grid.u[grid.idx_u(i, j, k)] = effective_speed;
                                grid.u[grid.idx_u(i + 1, j, k)] = effective_speed;
                            } else if (w.normal.x < -0.5) {
                                grid.u[grid.idx_u(i, j, k)] = -effective_speed;
                                grid.u[grid.idx_u(i + 1, j, k)] = -effective_speed;
                            } else if (w.normal.z > 0.5) {
                                grid.w[grid.idx_w(i, j, k)] = effective_speed;
                                grid.w[grid.idx_w(i, j, k + 1)] = effective_speed;
                            } else if (w.normal.z < -0.5) {
                                grid.w[grid.idx_w(i, j, k)] = -effective_speed;
                                grid.w[grid.idx_w(i, j, k + 1)] = -effective_speed;
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Human Hot Body Boundary (37 °C)
    for (const auto& h : occupants) {
        for (int i = 0; i < grid.nx; ++i) {
            double x = grid.cell_x(i);
            for (int j = 0; j < grid.ny; ++j) {
                double y = grid.cell_y(j);
                for (int k = 0; k < grid.nz; ++k) {
                    double z = grid.cell_z(k);
                    if (h.box.contains(x, y, z)) {
                        size_t c_idx = grid.idx_cell(i, j, k);
                        grid.T[c_idx] = h.temperature; // Strictly 37.0 °C
                        // No-slip velocity on occupant
                        grid.u[grid.idx_u(i, j, k)] = 0.0;
                        grid.u[grid.idx_u(i + 1, j, k)] = 0.0;
                        grid.v[grid.idx_v(i, j, k)] = 0.0;
                        grid.v[grid.idx_v(i, j + 1, k)] = 0.0;
                        grid.w[grid.idx_w(i, j, k)] = 0.0;
                        grid.w[grid.idx_w(i, j, k + 1)] = 0.0;
                    }
                }
            }
        }
    }
}

void BuildingGeometry::set_window_open(size_t window_idx, double ratio) {
    if (window_idx < windows.size()) {
        windows[window_idx].open_ratio = std::clamp(ratio, 0.0, 1.0);
    }
}

void BuildingGeometry::set_door_open(size_t door_idx, double ratio) {
    if (door_idx < doors.size()) {
        doors[door_idx].open_ratio = std::clamp(ratio, 0.0, 1.0);
    }
}

void BuildingGeometry::move_window(size_t window_idx, double center_x, double center_z) {
    if (window_idx >= windows.size()) return;
    auto& w = windows[window_idx];
    double width_x = w.box.x_max - w.box.x_min;
    double depth_z = w.box.z_max - w.box.z_min;

    if (std::abs(w.normal.x) > 0.5) {
        // Window on West or East wall (fixed X, movable Z)
        double half_z = depth_z * 0.5;
        double new_z = std::clamp(center_z, half_z + 0.2, 5.0 - half_z - 0.2);
        w.box.z_min = new_z - half_z;
        w.box.z_max = new_z + half_z;
    } else {
        // Window on South or North wall (fixed Z, movable X)
        double half_x = width_x * 0.5;
        double min_x = (w.room_id == 1) ? (half_x + 0.2) : (5.0 + half_x + 0.2);
        double max_x = (w.room_id == 1) ? (5.0 - half_x - 0.2) : (10.0 - half_x - 0.2);
        double new_x = std::clamp(center_x, min_x, max_x);
        w.box.x_min = new_x - half_x;
        w.box.x_max = new_x + half_x;
    }
}

void BuildingGeometry::move_door(size_t door_idx, double center_x, double center_z) {
    if (door_idx >= doors.size()) return;
    auto& d = doors[door_idx];
    double width_x = d.box.x_max - d.box.x_min;
    double depth_z = d.box.z_max - d.box.z_min;

    if (d.is_connecting) {
        // Partition door on middle wall X=5.0: moves along Z axis
        double half_z = depth_z * 0.5;
        double new_z = std::clamp(center_z, half_z + 0.3, 5.0 - half_z - 0.3);
        d.box.z_min = new_z - half_z;
        d.box.z_max = new_z + half_z;
    } else {
        // Exterior door on South wall Z=0: moves along X axis
        double half_x = width_x * 0.5;
        double new_x = std::clamp(center_x, 5.0 + half_x + 0.3, 10.0 - half_x - 0.3);
        d.box.x_min = new_x - half_x;
        d.box.x_max = new_x + half_x;
    }
}

void BuildingGeometry::set_occupant_temp(size_t occupant_idx, double temp_c) {
    if (occupant_idx < occupants.size()) {
        occupants[occupant_idx].temperature = temp_c;
    }
}

} // namespace cfd
