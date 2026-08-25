#include "SimulationEngine.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace cfd {

SimulationEngine::SimulationEngine(const SimulationConfig& cfg, const EnvironmentConfig& clim_cfg)
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
    grid.compute_wall_distances();
    stats.current_time = 0.0;
    stats.step_count = 0;
    update_statistics();
}

void SimulationEngine::sync_geometry_to_grid() {
    geometry.rasterize_to_grid(grid, climate);
    grid.compute_wall_distances();
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
    double sum_t = 0.0;
    double min_t = 1e9;
    double max_t = -1e9;
    size_t fluid_cells = 0;

    int nx = grid.nx, ny = grid.ny, nz = grid.nz;

    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            for (int k = 0; k < nz; ++k) {
                if (grid.is_solid(i, j, k)) continue;

                Vec3 v = grid.get_velocity_at_cell(i, j, k);
                double speed = v.norm();
                if (speed > max_v) max_v = speed;

                double temp = grid.T[grid.idx_cell(i, j, k)];
                sum_t += temp;
                if (temp < min_t) min_t = temp;
                if (temp > max_t) max_t = temp;
                fluid_cells++;
            }
        }
    }

    stats.max_velocity = max_v;
    stats.avg_temperature = (fluid_cells > 0) ? (sum_t / fluid_cells) : climate.config.outdoor_temp;
    stats.min_temperature = (fluid_cells > 0) ? min_t : climate.config.outdoor_temp;
    stats.max_temperature = (fluid_cells > 0) ? max_t : 37.0;

    double max_div = 0.0;
    for (int i = 1; i < nx - 1; ++i) {
        for (int j = 1; j < ny - 1; ++j) {
            for (int k = 1; k < nz - 1; ++k) {
                if (grid.is_solid(i, j, k)) continue;
                double div = grid.divergence(i, j, k);
                if (div > max_div) max_div = div;
            }
        }
    }
    stats.max_divergence = max_div;
}

// Dynamic Environmental Setters
void SimulationEngine::set_wind_speed(double speed) {
    climate.config.wind_speed_ref = std::max(0.0, speed);
}

void SimulationEngine::set_wind_angle(double angle_deg) {
    climate.config.wind_angle_deg = angle_deg;
}

void SimulationEngine::set_outdoor_temp(double temp_c) {
    climate.config.outdoor_temp = temp_c;
}

void SimulationEngine::set_paused(bool paused) {
    is_paused.store(paused);
    stats.is_paused = paused;
}

void SimulationEngine::set_substeps(int steps) {
    sub_steps = std::clamp(steps, 1, 16);
}

// --- Dynamic CAD Geometry Operations ---
std::string SimulationEngine::get_design_json() const {
    return geometry.export_to_json();
}

bool SimulationEngine::load_design_json(const std::string& json_str) {
    bool ok = geometry.import_from_json(json_str);
    if (ok) {
        sync_geometry_to_grid();
    }
    return ok;
}

int SimulationEngine::add_wall(double x1, double z1, double x2, double z2, double thickness) {
    int id = geometry.add_wall(x1, z1, x2, z2, thickness, true);
    sync_geometry_to_grid();
    return id;
}

bool SimulationEngine::delete_wall(int id) {
    bool ok = geometry.delete_wall(id);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::move_wall(int id, double x1, double z1, double x2, double z2) {
    bool ok = geometry.move_wall(id, x1, z1, x2, z2);
    if (ok) sync_geometry_to_grid();
    return ok;
}

int SimulationEngine::add_window(double x, double y, double z, double width, double height, Vec3 normal, double open_ratio) {
    int id = geometry.add_window(x, y, z, width, height, normal, open_ratio);
    sync_geometry_to_grid();
    return id;
}

bool SimulationEngine::delete_window(int id) {
    bool ok = geometry.delete_window(id);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::move_window_3d(int id, double x, double y, double z) {
    bool ok = geometry.move_window_3d(id, x, y, z);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::resize_window(int id, double width, double height) {
    bool ok = geometry.resize_window(id, width, height);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::set_window_state(int id, double open_ratio) {
    bool ok = geometry.set_window_open(id, open_ratio);
    if (ok) sync_geometry_to_grid();
    return ok;
}

int SimulationEngine::add_door(double x, double z, double width, double height, Vec3 normal, double open_ratio, bool is_connecting) {
    int id = geometry.add_door(x, z, width, height, normal, open_ratio, is_connecting);
    sync_geometry_to_grid();
    return id;
}

bool SimulationEngine::delete_door(int id) {
    bool ok = geometry.delete_door(id);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::move_door(int id, double x, double z) {
    bool ok = geometry.move_door(id, x, z);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::resize_door(int id, double width, double height) {
    bool ok = geometry.resize_door(id, width, height);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::set_door_state(int id, double open_ratio) {
    bool ok = geometry.set_door_open(id, open_ratio);
    if (ok) sync_geometry_to_grid();
    return ok;
}

int SimulationEngine::add_occupant(double x, double z, double temp_c) {
    int id = geometry.add_occupant(x, z, temp_c);
    sync_geometry_to_grid();
    return id;
}

bool SimulationEngine::delete_occupant(int id) {
    bool ok = geometry.delete_occupant(id);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::move_occupant(int id, double x, double z) {
    bool ok = geometry.move_occupant(id, x, z);
    if (ok) sync_geometry_to_grid();
    return ok;
}

bool SimulationEngine::set_human_temp(int id, double temp_c) {
    bool ok = geometry.set_occupant_temp(id, temp_c);
    if (ok) sync_geometry_to_grid();
    return ok;
}

// Probes & Serialization
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
       << "\"is_paused\":" << (stats.is_paused ? "true" : "false") << ","
       << "\"sub_steps\":" << sub_steps << ","
       << "\"wind_speed\":" << climate.config.wind_speed_ref << ","
       << "\"wind_angle\":" << climate.config.wind_angle_deg << ","
       << "\"outdoor_temp\":" << climate.config.outdoor_temp
       << "}";
    return ss.str();
}

std::string SimulationEngine::get_slice_json(int axis, int index) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    if (axis == 1) { // Y-slice (Planar Top-Down Floor Plan)
        int j = std::clamp(index, 0, grid.ny - 1);
        ss << "{"
           << "\"axis\":1,"
           << "\"index\":" << j << ","
           << "\"coord_val\":" << grid.cell_y(j) << ","
           << "\"dim1_name\":\"X\","
           << "\"dim2_name\":\"Z\","
           << "\"n1\":" << grid.nx << ","
           << "\"n2\":" << grid.nz << ","
           << "\"l1\":" << grid.lx << ","
           << "\"l2\":" << grid.lz << ","
           << "\"temperature\":[";

        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.x;
            }
        }
        ss << "],\"v\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.z; // Flow along Z (vertical in screen)
            }
        }
        ss << "],\"speed\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int k = 0; k < grid.nz; ++k) {
            for (int i = 0; i < grid.nx; ++i) {
                if (k > 0 || i > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    } else if (axis == 2) { // Z-slice (Front View XxY)
        int k = std::clamp(index, 0, grid.nz - 1);
        ss << "{"
           << "\"axis\":2,"
           << "\"index\":" << k << ","
           << "\"coord_val\":" << grid.cell_z(k) << ","
           << "\"dim1_name\":\"X\","
           << "\"dim2_name\":\"Y\","
           << "\"n1\":" << grid.nx << ","
           << "\"n2\":" << grid.ny << ","
           << "\"l1\":" << grid.lx << ","
           << "\"l2\":" << grid.ly << ","
           << "\"temperature\":[";

        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.x;
            }
        }
        ss << "],\"v\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.y;
            }
        }
        ss << "],\"speed\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                if (j > 0 || i > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    } else { // X-slice (Side View YxZ)
        int i = std::clamp(index, 0, grid.nx - 1);
        ss << "{"
           << "\"axis\":0,"
           << "\"index\":" << i << ","
           << "\"coord_val\":" << grid.cell_x(i) << ","
           << "\"dim1_name\":\"Z\","
           << "\"dim2_name\":\"Y\","
           << "\"n1\":" << grid.nz << ","
           << "\"n2\":" << grid.ny << ","
           << "\"l1\":" << grid.lz << ","
           << "\"l2\":" << grid.ly << ","
           << "\"temperature\":[";

        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                ss << grid.T[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"u\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.z;
            }
        }
        ss << "],\"v\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.y;
            }
        }
        ss << "],\"speed\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                Vec3 vel = grid.get_velocity_at_cell(i, j, k);
                ss << vel.norm();
            }
        }
        ss << "],\"pressure\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                ss << grid.p[grid.idx_cell(i, j, k)];
            }
        }
        ss << "],\"cell_type\":[";
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (j > 0 || k > 0) ss << ",";
                ss << static_cast<int>(grid.cell_type[grid.idx_cell(i, j, k)]);
            }
        }
        ss << "]}";
    }

    return ss.str();
}

std::string SimulationEngine::get_3d_state_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "{"
       << "\"nx\":" << grid.nx << ","
       << "\"ny\":" << grid.ny << ","
       << "\"nz\":" << grid.nz << ","
       << "\"lx\":" << grid.lx << ","
       << "\"ly\":" << grid.ly << ","
       << "\"lz\":" << grid.lz << ","
       << "\"walls\":[";

    for (size_t w = 0; w < geometry.walls.size(); ++w) {
        const auto& wl = geometry.walls[w];
        if (w > 0) ss << ",";
        ss << "{\"id\":" << wl.id
           << ",\"name\":\"" << wl.name << "\""
           << ",\"x1\":" << wl.x1 << ",\"z1\":" << wl.z1
           << ",\"x2\":" << wl.x2 << ",\"z2\":" << wl.z2
           << ",\"thickness\":" << wl.thickness
           << ",\"height\":" << wl.height
           << ",\"is_exterior\":" << (wl.is_exterior ? "true" : "false") << "}";
    }

    ss << "],\"windows\":[";
    for (size_t w = 0; w < geometry.windows.size(); ++w) {
        const auto& win = geometry.windows[w];
        if (w > 0) ss << ",";
        AABB b = win.get_box();
        ss << "{\"id\":" << win.id
           << ",\"name\":\"" << win.name << "\""
           << ",\"room_id\":" << win.room_id
           << ",\"x\":" << win.x << ",\"y\":" << win.y << ",\"z\":" << win.z
           << ",\"width\":" << win.width << ",\"height\":" << win.height
           << ",\"normal\":[" << win.normal.x << "," << win.normal.y << "," << win.normal.z << "]"
           << ",\"is_inflow\":" << (win.is_inflow ? "true" : "false")
           << ",\"open_ratio\":" << win.open_ratio
           << ",\"open_angle_deg\":" << (win.open_ratio * 90.0)
           << ",\"box\":[" << b.x_min << "," << b.x_max << ","
           << b.y_min << "," << b.y_max << ","
           << b.z_min << "," << b.z_max << "]}";
    }

    ss << "],\"doors\":[";
    for (size_t d = 0; d < geometry.doors.size(); ++d) {
        const auto& dor = geometry.doors[d];
        if (d > 0) ss << ",";
        AABB b = dor.get_box();
        ss << "{\"id\":" << dor.id
           << ",\"name\":\"" << dor.name << "\""
           << ",\"room_id\":" << dor.room_id
           << ",\"x\":" << dor.x << ",\"y\":" << dor.y << ",\"z\":" << dor.z
           << ",\"width\":" << dor.width << ",\"height\":" << dor.height
           << ",\"open_ratio\":" << dor.open_ratio
           << ",\"open_angle_deg\":" << (dor.open_ratio * 90.0)
           << ",\"is_connecting\":" << (dor.is_connecting ? "true" : "false")
           << ",\"box\":[" << b.x_min << "," << b.x_max << ","
           << b.y_min << "," << b.y_max << ","
           << b.z_min << "," << b.z_max << "]}";
    }

    ss << "],\"occupants\":[";
    for (size_t o = 0; o < geometry.occupants.size(); ++o) {
        const auto& occ = geometry.occupants[o];
        if (o > 0) ss << ",";
        AABB b = occ.get_box();
        ss << "{\"id\":" << occ.id
           << ",\"name\":\"" << occ.name << "\""
           << ",\"room_id\":" << occ.room_id
           << ",\"x\":" << occ.x << ",\"z\":" << occ.z
           << ",\"height\":" << occ.height
           << ",\"width\":" << occ.width
           << ",\"depth\":" << occ.depth
           << ",\"temperature\":" << occ.temperature
           << ",\"box\":[" << b.x_min << "," << b.x_max << ","
           << b.y_min << "," << b.y_max << ","
           << b.z_min << "," << b.z_max << "]}";
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
