#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "Geometry.hpp"
#include "EnvironmentClimate.hpp"
#include "Turbulence.hpp"
#include "NavierStokesSolver.hpp"
#include "ThermalSolver.hpp"
#include "VTKExporter.hpp"
#include <mutex>
#include <atomic>
#include <string>

namespace cfd {

struct SimulationStats {
    double current_time = 0.0;
    uint64_t step_count = 0;
    double current_dt = 0.01;
    double max_velocity = 0.0;
    double avg_temperature = 0.0;
    double min_temperature = 0.0;
    double max_temperature = 0.0;
    double max_divergence = 0.0;
    double computation_time_ms = 0.0;
    bool is_paused = false;
};

class SimulationEngine {
public:
    SimulationConfig config;
    PhysicalProperties props;
    EnvironmentClimate climate;
    BuildingGeometry geometry;
    Grid3D grid;
    TurbulenceModel turbulence;
    NavierStokesSolver ns_solver;
    ThermalSolver thermal_solver;

    SimulationStats stats;
    mutable std::mutex state_mutex;
    int sub_steps = 4;             // Multi-substepping for real-time physical playback
    std::atomic<bool> is_running{false};
    std::atomic<bool> is_paused{false};

    SimulationEngine(const SimulationConfig& cfg = SimulationConfig(),
                     const EnvironmentConfig& clim_cfg = EnvironmentConfig());

    void reset();
    void step();
    void step_multi(int n_substeps = 4);
    void run_steps(int count);

    // Dynamic environmental parameter controls
    void set_wind_speed(double speed);
    void set_wind_angle(double angle_deg);
    void set_outdoor_temp(double temp_c);
    void set_paused(bool paused);
    void set_substeps(int steps);

    // Dynamic CAD Geometry Operations
    std::string get_design_json() const;
    bool load_design_json(const std::string& json_str);

    int add_wall(double x1, double z1, double x2, double z2, double thickness = 0.20);
    bool delete_wall(int id);
    bool move_wall(int id, double x1, double z1, double x2, double z2);

    int add_window(double x, double y, double z, double width, double height, Vec3 normal = Vec3(1,0,0), double open_ratio = 1.0);
    bool delete_window(int id);
    bool move_window_3d(int id, double x, double y, double z);
    bool resize_window(int id, double width, double height);
    bool set_window_state(int id, double open_ratio);

    int add_door(double x, double z, double width = 1.0, double height = 2.1, Vec3 normal = Vec3(1,0,0), double open_ratio = 1.0, bool is_connecting = true);
    bool delete_door(int id);
    bool move_door(int id, double x, double z);
    bool resize_door(int id, double width, double height);
    bool set_door_state(int id, double open_ratio);

    int add_occupant(double x, double z, double temp_c = 37.0);
    bool delete_occupant(int id);
    bool move_occupant(int id, double x, double z);
    bool set_human_temp(int id, double temp_c);

    // Extract 2D slice data for visualization (axis: 0=X, 1=Y, 2=Z)
    std::string get_slice_json(int axis, int index) const;

    // Extract 3D volume summary data (walls, windows, doors, 6ft occupants, flow particles)
    std::string get_3d_state_json() const;

    // Query live probe at spatial point (x, y, z)
    ProbeData query_probe(double x, double y, double z) const;
    std::string get_probe_json(double x, double y, double z) const;

    // Get simulation metadata and live statistics
    std::string get_stats_json() const;

    // Export VTK file
    bool export_vtk(const std::string& filename);

private:
    double compute_adaptive_dt() const;
    void update_statistics();
    void sync_geometry_to_grid();
};

} // namespace cfd
