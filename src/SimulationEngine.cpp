#include "SimulationEngine.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace cfd {

SimulationEngine::SimulationEngine(const SimulationConfig& cfg, const ClimateConfig& clim_cfg)
    : config(cfg),
      climate(clim_cfg),
      geometry(cfg.lx, cfg.ly, cfg.lz),
      grid(cfg.nx, cfg.ny, cfg.nz, cfg.lx, cfg.ly, cfg.lz),
      turbulence(props),
      ns_solver(props, cfg),
      thermal_solver(props) {
    
    geometry.rasterize_to_grid(grid, climate);
    update_statistics();
}

void SimulationEngine::reset() {
    grid.reset();
    geometry.rasterize_to_grid(grid, climate);
    stats.current_time = 0.0;
    stats.step_count = 0;
    update_statistics();
}

double SimulationEngine::compute_adaptive_dt() const {
    double max_v = 0.1;
    int nx = grid.nx, ny = grid.ny, nz = grid.nz;

    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            for (int k = 0; k < nz; ++k) {
                if (grid.is_solid(i, j, k)) continue;
                Vec3 v = grid.get_velocity_at_cell(i, j, k);
                double speed = v.norm();
                if (speed > max_v) max_v = speed;
            }
        }
    }

    double min_dx = std::min({grid.dx, grid.dy, grid.dz});
    double dt_cfl = config.max_cfl * (min_dx / max_v);
    double max_diff = props.nu + 0.005;
    double dt_diff = 0.45 * (min_dx * min_dx) / max_diff;

    return std::clamp(std::min(dt_cfl, dt_diff), 0.003, 0.035);
}

void SimulationEngine::step() {
    auto t_start = std::chrono::high_resolution_clock::now();

    double dt = compute_adaptive_dt();
    stats.current_dt = dt;

    // 1. Enforce geometric boundary conditions
    geometry.enforce_boundary_conditions(grid, climate, stats.current_time);

    // 2. Smagorinsky LES Subgrid-Scale turbulence
    if (config.enable_les) {
        turbulence.compute_eddy_viscosity(grid);
    }

    // 3. Momentum Navier-Stokes equations
    ns_solver.step(grid, climate, dt);

    // 4. Thermal Energy equation
    thermal_solver.step(grid, climate, dt);

    // 5. Re-enforce Dirichlet boundary conditions
    geometry.enforce_boundary_conditions(grid, climate, stats.current_time);

    stats.current_time += dt;
    stats.step_count++;

    auto t_end = std::chrono::high_resolution_clock::now();
    stats.computation_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    update_statistics();
}

void SimulationEngine::step_multi(int n_substeps) {
    auto t_start = std::chrono::high_resolution_clock::now();
    int count = std::max(1, n_substeps);

    for (int s = 0; s < count; ++s) {
        double dt = compute_adaptive_dt();
        stats.current_dt = dt;

        geometry.enforce_boundary_conditions(grid, climate, stats.current_time);
        if (config.enable_les) {
            turbulence.compute_eddy_viscosity(grid);
        }
        ns_solver.step(grid, climate, dt);
        thermal_solver.step(grid, climate, dt);
        geometry.enforce_boundary_conditions(grid, climate, stats.current_time);

        stats.current_time += dt;
        stats.step_count++;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    stats.computation_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    update_statistics();
}

void SimulationEngine::run_steps(int count) {
    for (int i = 0; i < count; ++i) {
        step();
    }
}

void SimulationEngine::update_statistics() {
    double max_v = 0.0;
    double min_t = 1e9;
    double max_t = -1e9;
    double sum_t = 0.0;
    int fluid_count = 0;
    double max_div = 0.0;

    int nx = grid.nx, ny = grid.ny, nz = grid.nz;

    for (int i = 0; i < nx; ++i) {
        int iny = i * ny;
        for (int j = 0; j < ny; ++j) {
            int base = (iny + j) * nz;
            for (int k = 0; k < nz; ++k) {
                size_t c_idx = base + k;
                if (grid.cell_type[c_idx] == CELL_SOLID_WALL) continue;

                double temp = grid.T[c_idx];
                if (temp < min_t) min_t = temp;
                if (temp > max_t) max_t = temp;
                sum_t += temp;
                fluid_count++;

                Vec3 v = grid.get_velocity_at_cell(i, j, k);
                double speed = v.norm();
                if (speed > max_v) max_v = speed;

                double d = std::abs(grid.div[c_idx]);
                if (d > max_div) max_div = d;
            }
        }
    }

    stats.max_velocity = max_v;
    stats.min_temperature = (fluid_count > 0) ? min_t : 32.0;
    stats.max_temperature = (fluid_count > 0) ? max_t : 37.0;
    stats.avg_temperature = (fluid_count > 0) ? (sum_t / fluid_count) : 32.0;
    stats.max_divergence = max_div;
    stats.is_paused = is_paused.load();
}

void SimulationEngine::set_wind_speed(double speed) {
    climate.config.wind_speed_ref = std::max(0.0, speed);
}

void SimulationEngine::set_wind_angle(double angle_deg) {
    climate.config.wind_angle_deg = angle_deg;
}

void SimulationEngine::set_outdoor_temp(double temp_c) {
    climate.config.outdoor_temp = temp_c;
}

void SimulationEngine::set_human_temp(int room_idx, double temp_c) {
    if (room_idx >= 0 && static_cast<size_t>(room_idx) < geometry.occupants.size()) {
        geometry.set_occupant_temp(room_idx, temp_c);
    }
}

void SimulationEngine::set_window_state(size_t window_idx, double open_ratio) {
    geometry.set_window_open(window_idx, open_ratio);
    geometry.rasterize_to_grid(grid, climate);
}

void SimulationEngine::set_door_state(size_t door_idx, double open_ratio) {
    geometry.set_door_open(door_idx, open_ratio);
    geometry.rasterize_to_grid(grid, climate);
}

void SimulationEngine::move_window(size_t window_idx, double center_x, double center_z) {
    geometry.move_window(window_idx, center_x, center_z);
    geometry.rasterize_to_grid(grid, climate);
    grid.compute_wall_distances();
}

void SimulationEngine::move_door(size_t door_idx, double center_x, double center_z) {
    geometry.move_door(door_idx, center_x, center_z);
    geometry.rasterize_to_grid(grid, climate);
    grid.compute_wall_distances();
}

void SimulationEngine::set_paused(bool paused) {
    is_paused.store(paused);
    stats.is_paused = paused;
}

void SimulationEngine::set_substeps(int steps) {
    sub_steps = std::clamp(steps, 1, 16);
}

ProbeData SimulationEngine::query_probe(double x, double y, double z) const {
    return grid.query_probe(x, y, z);
}

std::string SimulationEngine::get_probe_json(double x, double y, double z) const {
    ProbeData p = query_probe(x, y, z);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"x\":" << p.x << ","
       << "\"y\":" << p.y << ","
       << "\"z\":" << p.z << ","
       << "\"i\":" << p.i << ","
       << "\"j\":" << p.j << ","
       << "\"k\":" << p.k << ","
       << "\"temperature\":" << p.temperature << ","
       << "\"u\":" << p.u << ","
       << "\"v\":" << p.v << ","
       << "\"w\":" << p.w << ","
       << "\"speed\":" << p.speed << ","
       << "\"pressure\":" << p.pressure << ","
       << "\"eddy_viscosity\":" << p.eddy_viscosity << ","
       << "\"cell_type\":" << static_cast<int>(p.cell_type)
       << "}";
    return ss.str();
}

std::string SimulationEngine::get_stats_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{"
       << "\"time\":" << stats.current_time << ","
       << "\"step\":" << stats.step_count << ","
       << "\"dt\":" << stats.current_dt << ","
       << "\"max_velocity\":" << stats.max_velocity << ","
       << "\"avg_temp\":" << stats.avg_temperature << ","
       << "\"min_temp\":" << stats.min_temperature << ","
       << "\"max_temp\":" << stats.max_temperature << ","
       << "\"max_divergence\":" << stats.max_divergence << ","
       << "\"compute_time_ms\":" << stats.computation_time_ms << ","
       << "\"sub_steps\":" << sub_steps << ","
       << "\"is_paused\":" << (stats.is_paused ? "true" : "false") << ","
       << "\"wind_speed\":" << climate.config.wind_speed_ref << ","
       << "\"wind_angle\":" << climate.config.wind_angle_deg << ","
       << "\"outdoor_temp\":" << climate.config.outdoor_temp
       << "}";
    return ss.str();
}

std::string SimulationEngine::get_slice_json(int axis, int index) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    if (axis == 1) { // Y slice (Top-down floor plan)
        int j = std::clamp(index, 0, grid.ny - 1);
        double slice_y = grid.cell_y(j);

        ss << "{"
           << "\"axis\":1,"
           << "\"index\":" << j << ","
           << "\"coord_val\":" << slice_y << ","
           << "\"dim1_name\":\"X\","
           << "\"dim2_name\":\"Z\","
           << "\"n1\":" << grid.nx << ","
           << "\"n2\":" << grid.nz << ","
           << "\"l1\":" << grid.lx << ","
           << "\"l2\":" << grid.lz << ","
           << "\"temperature\":[";

        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.x;
            }
        }
        ss << "],\"v\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.z;
            }
        }
        ss << "],\"speed\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || k > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    } else if (axis == 2) { // Z slice
        int k = std::clamp(index, 0, grid.nz - 1);
        double slice_z = grid.cell_z(k);

        ss << "{"
           << "\"axis\":2,"
           << "\"index\":" << k << ","
           << "\"coord_val\":" << slice_z << ","
           << "\"dim1_name\":\"X\","
           << "\"dim2_name\":\"Y\","
           << "\"n1\":" << grid.nx << ","
           << "\"n2\":" << grid.ny << ","
           << "\"l1\":" << grid.lx << ","
           << "\"l2\":" << grid.ly << ","
           << "\"temperature\":[";

        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.x;
            }
        }
        ss << "],\"v\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.y;
            }
        }
        ss << "],\"speed\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (i > 0 || j > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    } else { // X slice
        int i = std::clamp(index, 0, grid.nx - 1);
        double slice_x = grid.cell_x(i);

        ss << "{"
           << "\"axis\":0,"
           << "\"index\":" << i << ","
           << "\"coord_val\":" << slice_x << ","
           << "\"dim1_name\":\"Z\","
           << "\"dim2_name\":\"Y\","
           << "\"n1\":" << grid.nz << ","
           << "\"n2\":" << grid.ny << ","
           << "\"l1\":" << grid.lz << ","
           << "\"l2\":" << grid.ly << ","
           << "\"temperature\":[";

        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.z;
            }
        }
        ss << "],\"v\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.y;
            }
        }
        ss << "],\"speed\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (k > 0 || j > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    }

    return ss.str();
}

std::string SimulationEngine::get_3d_state_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "{"
       << "\"nx\":" << grid.nx << ","
       << "\"ny\":" << grid.ny << ","
       << "\"nz\":" << grid.nz << ","
       << "\"lx\":" << grid.lx << ","
       << "\"ly\":" << grid.ly << ","
       << "\"lz\":" << grid.lz << ","
       << "\"rooms\":[";

    for (size_t r = 0; r < geometry.rooms.size(); ++r) {
        const auto& rm = geometry.rooms[r];
        if (r > 0) ss << ",";
        ss << "{\"id\":" << rm.id
           << ",\"name\":\"" << rm.name << "\""
           << ",\"box\":[" << rm.bounds.x_min << "," << rm.bounds.x_max << ","
           << rm.bounds.y_min << "," << rm.bounds.y_max << ","
           << rm.bounds.z_min << "," << rm.bounds.z_max << "]}";
    }

    ss << "],\"windows\":[";
    for (size_t w = 0; w < geometry.windows.size(); ++w) {
        const auto& win = geometry.windows[w];
        if (w > 0) ss << ",";
        ss << "{\"name\":\"" << win.name << "\""
           << ",\"room_id\":" << win.room_id
           << ",\"is_inflow\":" << (win.is_inflow ? "true" : "false")
           << ",\"open_ratio\":" << win.open_ratio
           << ",\"box\":[" << win.box.x_min << "," << win.box.x_max << ","
           << win.box.y_min << "," << win.box.y_max << ","
           << win.box.z_min << "," << win.box.z_max << "]}";
    }

    ss << "],\"doors\":[";
    for (size_t d = 0; d < geometry.doors.size(); ++d) {
        const auto& dor = geometry.doors[d];
        if (d > 0) ss << ",";
        ss << "{\"name\":\"" << dor.name << "\""
           << ",\"room_id\":" << dor.room_id
           << ",\"open_ratio\":" << dor.open_ratio
           << ",\"box\":[" << dor.box.x_min << "," << dor.box.x_max << ","
           << dor.box.y_min << "," << dor.box.y_max << ","
           << dor.box.z_min << "," << dor.box.z_max << "]}";
    }

    ss << "],\"occupants\":[";
    for (size_t o = 0; o < geometry.occupants.size(); ++o) {
        const auto& occ = geometry.occupants[o];
        if (o > 0) ss << ",";
        ss << "{\"name\":\"" << occ.name << "\""
           << ",\"room_id\":" << occ.room_id
           << ",\"temperature\":" << occ.temperature
           << ",\"box\":[" << occ.box.x_min << "," << occ.box.x_max << ","
           << occ.box.y_min << "," << occ.box.y_max << ","
           << occ.box.z_min << "," << occ.box.z_max << "]}";
    }

    ss << "],\"particles\":[";
    int step_i = std::max(1, grid.nx / 16);
    int step_j = std::max(1, grid.ny / 8);
    int step_k = std::max(1, grid.nz / 10);
    bool first_p = true;

    for (int i = 1; i < grid.nx - 1; i += step_i) {
        for (int j = 1; j < grid.ny - 1; j += step_j) {
            for (int k = 1; k < grid.nz - 1; k += step_k) {
                if (grid.is_solid(i, j, k)) continue;
                if (!first_p) ss << ",";
                first_p = false;

                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                double t = grid.T[grid.idx_cell(i, j, k)];
                ss << "[" << grid.cell_x(i) << "," << grid.cell_y(j) << "," << grid.cell_z(k) << ","
                   << vel.x << "," << vel.y << "," << vel.z << "," << t << "]";
            }
        }
    }
    ss << "]}";

    return ss.str();
}

bool SimulationEngine::export_vtk(const std::string& filename) {
    return VTKExporter::export_legacy_vtk(grid, filename, stats.current_time);
}

} // namespace cfd
