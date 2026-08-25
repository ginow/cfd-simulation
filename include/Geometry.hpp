#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "KeralaClimate.hpp"
#include <vector>
#include <string>

namespace cfd {

struct AABB {
    double x_min, x_max;
    double y_min, y_max;
    double z_min, z_max;

    bool contains(double x, double y, double z) const {
        return (x >= x_min && x <= x_max &&
                y >= y_min && y <= y_max &&
                z >= z_min && z <= z_max);
    }
};

struct Window {
    std::string name;
    int room_id;
    AABB box;
    Vec3 normal;             // Outward normal vector (+/-X or +/-Z)
    double open_ratio = 1.0;  // 0.0 = closed, 1.0 = fully open
    bool is_inflow = true;    // True if windward inflow, false if leeward outflow
};

struct Door {
    std::string name;
    int room_id;
    AABB box;
    double open_ratio = 1.0;  // 0.0 = closed, 1.0 = open
    bool is_connecting = true;// True if connects Room 1 and Room 2
};

struct HumanBody {
    std::string name;
    int room_id;
    AABB box;
    double temperature = 37.0; // °C (310.15 K)
    double metabolic_rate = 90.0; // Sensible heat generation [Watts]
};

struct Room {
    int id;
    std::string name;
    AABB bounds;
};

class BuildingGeometry {
public:
    std::vector<Room> rooms;
    std::vector<Window> windows;
    std::vector<Door> doors;
    std::vector<HumanBody> occupants;

    BuildingGeometry(double lx = 10.0, double ly = 3.0, double lz = 5.0);

    // Voxelize building architecture onto staggered 3D grid
    void rasterize_to_grid(Grid3D& grid, const KeralaClimate& climate);

    // Apply thermal and velocity boundary conditions at all openings & surfaces
    void enforce_boundary_conditions(Grid3D& grid, const KeralaClimate& climate, double time);

    // Set window open ratio dynamically
    void set_window_open(size_t window_idx, double ratio);

    // Set door open ratio dynamically
    void set_door_open(size_t door_idx, double ratio);

    // Move window along its wall (center coordinates in meters)
    void move_window(size_t window_idx, double center_x, double center_z);

    // Move door along its wall (center coordinates in meters)
    void move_door(size_t door_idx, double center_x, double center_z);

    // Set human body temperature
    void set_occupant_temp(size_t occupant_idx, double temp_c);
};

} // namespace cfd
