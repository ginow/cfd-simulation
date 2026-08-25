#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "Geometry.hpp"
#include "KeralaClimate.hpp"
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
    KeralaClimate climate;
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
                     const ClimateConfig& clim_cfg = ClimateConfig());

    void reset();
    void step();
    void step_multi(int n_substeps = 4);
    void run_steps(int count);

    // Dynamic parameter controls
    void set_wind_speed(double speed);
    void set_wind_angle(double angle_deg);
    void set_outdoor_temp(double temp_c);
    void set_human_temp(int room_idx, double temp_c);
    void set_window_state(size_t window_idx, double open_ratio);
    void set_door_state(size_t door_idx, double open_ratio);
    void move_window(size_t window_idx, double center_x, double center_z);
    void move_door(size_t door_idx, double center_x, double center_z);
    void set_paused(bool paused);
    void set_substeps(int steps);


    // Extract 2D slice data for visualization (axis: 0=X, 1=Y, 2=Z)
    std::string get_slice_json(int axis, int index) const;

    // Extract 3D volume summary data (sparse / downsampled or key isosurfaces and particle field)
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
};

} // namespace cfd
