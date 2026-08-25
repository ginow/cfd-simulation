#pragma once

#include "Common.hpp"
#include "Grid3D.hpp"
#include "EnvironmentClimate.hpp"
#include <vector>
#include <string>
#include <memory>

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

    double volume() const {
        return std::max(0.001, (x_max - x_min) * (y_max - y_min) * (z_max - z_min));
    }
};

// 1. Wall Primitive (2D horizontal line extruded vertically to fixed 2.80m height)
struct Wall {
    int id = 0;
    std::string name = "Wall";
    double x1 = 0.0, z1 = 0.0;
    double x2 = 0.0, z2 = 0.0;
    double thickness = 0.20; // 20 cm standard concrete/brick wall
    double height = 2.80;    // Fixed ceiling height = 280 cm (2.80 m)
    bool is_exterior = true;

    AABB get_bounding_box() const {
        double min_x = std::min(x1, x2) - thickness * 0.5;
        double max_x = std::max(x1, x2) + thickness * 0.5;
        double min_z = std::min(z1, z2) - thickness * 0.5;
        double max_z = std::max(z1, z2) + thickness * 0.5;
        return {min_x, max_x, 0.0, height, min_z, max_z};
    }

    bool contains(double x, double y, double z) const {
        if (y < 0.0 || y > height) return false;
        // Distance from point (x, z) to line segment (x1, z1)-(x2, z2)
        double dx = x2 - x1;
        double dz = z2 - z1;
        double l2 = dx * dx + dz * dz;
        if (l2 < 1e-6) {
            double d = std::sqrt((x - x1) * (x - x1) + (z - z1) * (z - z1));
            return d <= thickness * 0.5;
        }
        double t = std::max(0.0, std::min(1.0, ((x - x1) * dx + (z - z1) * dz) / l2));
        double proj_x = x1 + t * dx;
        double proj_z = z1 + t * dz;
        double dist = std::sqrt((x - proj_x) * (x - proj_x) + (z - proj_z) * (z - proj_z));
        return dist <= (thickness * 0.5 + 0.05); // slight tolerance for staggered grid
    }
};

// 2. Window (Adjustable width, height, sill elevation, horizontal position, open ratio & angle)
struct Window {
    int id = 0;
    std::string name = "Window";
    int room_id = 1;
    double x = 0.0;          // Center X [m]
    double y = 1.50;         // Center Y [m] (Sill elevation = y - height/2)
    double z = 2.50;         // Center Z [m]
    double width = 1.50;     // Window aperture width [m]
    double height = 1.20;    // Window aperture height [m]
    Vec3 normal = Vec3(1.0, 0.0, 0.0); // Outward wall normal (+/-X or +/-Z)
    double open_ratio = 1.0; // 0.0 (closed) to 1.0 (fully open)
    bool is_inflow = true;   // Computed automatically based on wind angle

    AABB get_box() const {
        double half_w = width * 0.5;
        double half_h = height * 0.5;
        double depth_th = 0.35; // Wall thickness overlap

        if (std::abs(normal.x) > 0.5) { // West or East Wall
            return {x - depth_th * 0.5, x + depth_th * 0.5,
                    std::max(0.0, y - half_h), std::min(2.80, y + half_h),
                    z - half_w, z + half_w};
        } else { // South or North Wall
            return {x - half_w, x + half_w,
                    std::max(0.0, y - half_h), std::min(2.80, y + half_h),
                    z - depth_th * 0.5, z + depth_th * 0.5};
        }
    }
};

// 3. Door (Adjustable width, height, position, open ratio & 90° swing angle)
struct Door {
    int id = 0;
    std::string name = "Door";
    int room_id = 1;
    double x = 5.0;          // Center X [m]
    double y = 1.05;         // Center Y [m] (height / 2)
    double z = 2.50;         // Center Z [m]
    double width = 1.00;     // Door aperture width [m]
    double height = 2.10;    // Door aperture height [m] (standard 2.1m)
    Vec3 normal = Vec3(1.0, 0.0, 0.0);
    double open_ratio = 1.0; // 0.0 (closed) to 1.0 (fully open)
    bool is_connecting = true;// True if partition door between indoor rooms

    AABB get_box() const {
        double half_w = width * 0.5;
        double depth_th = 0.35;

        if (std::abs(normal.x) > 0.5) {
            return {x - depth_th * 0.5, x + depth_th * 0.5,
                    0.0, height,
                    z - half_w, z + half_w};
        } else {
            return {x - half_w, x + half_w,
                    0.0, height,
                    z - depth_th * 0.5, z + depth_th * 0.5};
        }
    }
};

// 4. Human Body Dummy (Fixed 6 feet = 1.83 m, 37.0 °C, 90W Metabolic heat, Draggable across floor)
struct HumanBody {
    int id = 0;
    std::string name = "Human Occupant (6ft Dummy @ 37°C)";
    int room_id = 1;
    double x = 2.5;          // Floor position X [m]
    double z = 2.5;          // Floor position Z [m]
    double height = 1.83;    // Exactly 6 feet (1.8288 m ~ 1.83 m)
    double width = 0.45;     // Shoulder width [m]
    double depth = 0.30;     // Torso depth [m]
    double temperature = 37.0;// °C (310.15 K)
    double metabolic_rate = 90.0; // 90 Watts sensible heat

    AABB get_box() const {
        return {x - width * 0.5, x + width * 0.5,
                0.0, height,
                z - depth * 0.5, z + depth * 0.5};
    }
};

// 5. Room bounds container
struct Room {
    int id = 0;
    std::string name = "Room";
    AABB bounds;
};

// Master Building Geometry CAD & Voxelizer Engine
class BuildingGeometry {
public:
    double lx = 10.0;
    double ly = 2.80; // Standard 2.80 m fixed room ceiling height
    double lz = 5.0;

    std::vector<Wall> walls;
    std::vector<Window> windows;
    std::vector<Door> doors;
    std::vector<HumanBody> occupants;
    std::vector<Room> rooms;

    int next_wall_id = 1;
    int next_win_id = 1;
    int next_door_id = 1;
    int next_occ_id = 1;

    BuildingGeometry(double _lx = 10.0, double _ly = 2.80, double _lz = 5.0);

    void reset_to_default_two_room();

    // --- Dynamic CAD Editing Operations ---
    int add_wall(double x1, double z1, double x2, double z2, double thickness = 0.20, bool is_ext = true);
    bool delete_wall(int id);
    bool move_wall(int id, double x1, double z1, double x2, double z2);

    int add_window(double x, double y, double z, double width, double height, Vec3 normal, double open_ratio = 1.0);
    bool delete_window(int id);
    bool move_window_3d(int id, double x, double y, double z);
    bool resize_window(int id, double width, double height);
    bool set_window_open(int id, double ratio);

    int add_door(double x, double z, double width, double height, Vec3 normal, double open_ratio = 1.0, bool is_connecting = true);
    bool delete_door(int id);
    bool move_door(int id, double x, double z);
    bool resize_door(int id, double width, double height);
    bool set_door_open(int id, double ratio);

    int add_occupant(double x, double z, double temp_c = 37.0);
    bool delete_occupant(int id);
    bool move_occupant(int id, double x, double z);
    bool set_occupant_temp(int id, double temp_c);

    // --- JSON Import / Export ---
    std::string export_to_json() const;
    bool import_from_json(const std::string& json_str);

    // --- CFD Grid Voxelization & Boundary Conditions ---
    void rasterize_to_grid(Grid3D& grid, const EnvironmentClimate& climate);
    void enforce_boundary_conditions(Grid3D& grid, const EnvironmentClimate& climate, double time);
};

} // namespace cfd
