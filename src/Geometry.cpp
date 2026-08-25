#include "Geometry.hpp"
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cfd {

BuildingGeometry::BuildingGeometry(double _lx, double _ly, double _lz)
    : lx(_lx), ly(_ly), lz(_lz) {
    reset_to_default_two_room();
}

void BuildingGeometry::reset_to_default_two_room() {
    walls.clear();
    windows.clear();
    doors.clear();
    occupants.clear();
    rooms.clear();

    next_wall_id = 1;
    next_win_id = 1;
    next_door_id = 1;
    next_occ_id = 1;

    // 1. Exterior Walls and Interior Partition Wall (Height = 2.80 m)
    add_wall(0.0, 0.0, 0.0, lz, 0.20, true);        // Wall 1: West
    add_wall(0.0, lz, lx, lz, 0.20, true);         // Wall 2: North
    add_wall(lx, lz, lx, 0.0, 0.20, true);         // Wall 3: East
    add_wall(lx, 0.0, 0.0, 0.0, 0.20, true);         // Wall 4: South
    add_wall(lx * 0.5, 0.0, lx * 0.5, lz, 0.20, false); // Wall 5: Middle Partition

    // 2. Windows (Adjustable Width, Height, Elevation Y, Normal, Open Ratio)
    add_window(0.0, 1.50, 2.50, 1.50, 1.20, Vec3(1.0, 0.0, 0.0), 1.0);  // Room 1: West Window
    add_window(2.50, 1.50, 0.0, 1.50, 1.20, Vec3(0.0, 0.0, 1.0), 1.0);  // Room 1: South Window
    add_window(7.50, 1.50, lz, 1.50, 1.20, Vec3(0.0, 0.0, -1.0), 1.0); // Room 2: North Window
    add_window(lx, 1.50, 2.50, 1.50, 1.20, Vec3(1.0, 0.0, 0.0), 1.0);  // Room 2: East Window

    // 3. Doors (Adjustable Width, Height, Open Ratio)
    add_door(lx * 0.5, 2.50, 1.00, 2.10, Vec3(1.0, 0.0, 0.0), 1.0, true); // Partition Door
    add_door(7.00, 0.0, 1.00, 2.10, Vec3(0.0, 0.0, 1.0), 0.5, false);     // Room 2 Exterior Door

    // 4. Human Occupant Dummies (6 feet = 1.83 m height, 37.0 °C)
    add_occupant(2.50, 2.50, 37.0); // Room 1 Human
    add_occupant(7.50, 2.50, 37.0); // Room 2 Human

    // 5. Rooms
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
}

// --- Wall Operations ---
int BuildingGeometry::add_wall(double x1, double z1, double x2, double z2, double thickness, bool is_ext) {
    Wall w;
    w.id = next_wall_id++;
    w.name = "Wall " + std::to_string(w.id);
    w.x1 = x1; w.z1 = z1;
    w.x2 = x2; w.z2 = z2;
    w.thickness = thickness;
    w.height = 2.80; // Fixed 280 cm
    w.is_exterior = is_ext;
    walls.push_back(w);
    return w.id;
}

bool BuildingGeometry::delete_wall(int id) {
    auto it = std::find_if(walls.begin(), walls.end(), [id](const Wall& w) { return w.id == id; });
    if (it != walls.end()) {
        walls.erase(it);
        return true;
    }
    return false;
}

bool BuildingGeometry::move_wall(int id, double x1, double z1, double x2, double z2) {
    auto it = std::find_if(walls.begin(), walls.end(), [id](const Wall& w) { return w.id == id; });
    if (it != walls.end()) {
        it->x1 = x1; it->z1 = z1;
        it->x2 = x2; it->z2 = z2;
        return true;
    }
    return false;
}

// --- Window Operations ---
int BuildingGeometry::add_window(double x, double y, double z, double width, double height, Vec3 normal, double open_ratio) {
    Window w;
    w.id = next_win_id++;
    w.name = "Window " + std::to_string(w.id);
    w.x = x;
    w.y = std::clamp(y, height * 0.5, 2.80 - height * 0.5);
    w.z = z;
    w.width = width;
    w.height = height;
    w.normal = normal;
    w.open_ratio = std::clamp(open_ratio, 0.0, 1.0);
    w.is_inflow = true;
    windows.push_back(w);
    return w.id;
}

bool BuildingGeometry::delete_window(int id) {
    auto it = std::find_if(windows.begin(), windows.end(), [id](const Window& w) { return w.id == id; });
    if (it != windows.end()) {
        windows.erase(it);
        return true;
    }
    return false;
}

bool BuildingGeometry::move_window_3d(int id, double x, double y, double z) {
    auto it = std::find_if(windows.begin(), windows.end(), [id](const Window& w) { return w.id == id; });
    if (it != windows.end()) {
        it->x = x;
        it->y = std::clamp(y, it->height * 0.5 + 0.1, 2.80 - it->height * 0.5 - 0.1);
        it->z = z;
        return true;
    }
    return false;
}

bool BuildingGeometry::resize_window(int id, double width, double height) {
    auto it = std::find_if(windows.begin(), windows.end(), [id](const Window& w) { return w.id == id; });
    if (it != windows.end()) {
        it->width = std::clamp(width, 0.40, 4.0);
        it->height = std::clamp(height, 0.40, 2.40);
        it->y = std::clamp(it->y, it->height * 0.5 + 0.1, 2.80 - it->height * 0.5 - 0.1);
        return true;
    }
    return false;
}

bool BuildingGeometry::set_window_open(int id, double ratio) {
    auto it = std::find_if(windows.begin(), windows.end(), [id](const Window& w) { return w.id == id; });
    if (it != windows.end()) {
        it->open_ratio = std::clamp(ratio, 0.0, 1.0);
        return true;
    }
    // Fallback to index if id not found
    if (id >= 0 && static_cast<size_t>(id) < windows.size()) {
        windows[id].open_ratio = std::clamp(ratio, 0.0, 1.0);
        return true;
    }
    return false;
}

// --- Door Operations ---
int BuildingGeometry::add_door(double x, double z, double width, double height, Vec3 normal, double open_ratio, bool is_connecting) {
    Door d;
    d.id = next_door_id++;
    d.name = "Door " + std::to_string(d.id);
    d.x = x;
    d.y = height * 0.5;
    d.z = z;
    d.width = width;
    d.height = height;
    d.normal = normal;
    d.open_ratio = std::clamp(open_ratio, 0.0, 1.0);
    d.is_connecting = is_connecting;
    doors.push_back(d);
    return d.id;
}

bool BuildingGeometry::delete_door(int id) {
    auto it = std::find_if(doors.begin(), doors.end(), [id](const Door& d) { return d.id == id; });
    if (it != doors.end()) {
        doors.erase(it);
        return true;
    }
    return false;
}

bool BuildingGeometry::move_door(int id, double x, double z) {
    auto it = std::find_if(doors.begin(), doors.end(), [id](const Door& d) { return d.id == id; });
    if (it != doors.end()) {
        it->x = x;
        it->z = z;
        return true;
    }
    return false;
}

bool BuildingGeometry::resize_door(int id, double width, double height) {
    auto it = std::find_if(doors.begin(), doors.end(), [id](const Door& d) { return d.id == id; });
    if (it != doors.end()) {
        it->width = std::clamp(width, 0.60, 2.50);
        it->height = std::clamp(height, 1.50, 2.70);
        it->y = it->height * 0.5;
        return true;
    }
    return false;
}

bool BuildingGeometry::set_door_open(int id, double ratio) {
    auto it = std::find_if(doors.begin(), doors.end(), [id](const Door& d) { return d.id == id; });
    if (it != doors.end()) {
        doors[id].open_ratio = std::clamp(ratio, 0.0, 1.0);
        return true;
    }
    if (id >= 0 && static_cast<size_t>(id) < doors.size()) {
        doors[id].open_ratio = std::clamp(ratio, 0.0, 1.0);
        return true;
    }
    return false;
}

// --- Human Occupant Operations (6ft Dummy @ 37°C) ---
int BuildingGeometry::add_occupant(double x, double z, double temp_c) {
    HumanBody h;
    h.id = next_occ_id++;
    h.name = "Human Occupant " + std::to_string(h.id) + " (6ft @ " + std::to_string(static_cast<int>(temp_c)) + "°C)";
    h.x = x;
    h.z = z;
    h.height = 1.83; // Exactly 6 feet (1.83 m)
    h.width = 0.45;
    h.depth = 0.30;
    h.temperature = temp_c;
    h.metabolic_rate = 90.0;
    occupants.push_back(h);
    return h.id;
}

bool BuildingGeometry::delete_occupant(int id) {
    auto it = std::find_if(occupants.begin(), occupants.end(), [id](const HumanBody& h) { return h.id == id; });
    if (it != occupants.end()) {
        occupants.erase(it);
        return true;
    }
    return false;
}

bool BuildingGeometry::move_occupant(int id, double x, double z) {
    auto it = std::find_if(occupants.begin(), occupants.end(), [id](const HumanBody& h) { return h.id == id; });
    if (it != occupants.end()) {
        it->x = std::clamp(x, 0.3, lx - 0.3);
        it->z = std::clamp(z, 0.3, lz - 0.3);
        return true;
    }
    if (id >= 0 && static_cast<size_t>(id) < occupants.size()) {
        occupants[id].x = std::clamp(x, 0.3, lx - 0.3);
        occupants[id].z = std::clamp(z, 0.3, lz - 0.3);
        return true;
    }
    return false;
}

bool BuildingGeometry::set_occupant_temp(int id, double temp_c) {
    auto it = std::find_if(occupants.begin(), occupants.end(), [id](const HumanBody& h) { return h.id == id; });
    if (it != occupants.end()) {
        it->temperature = temp_c;
        return true;
    }
    if (id >= 0 && static_cast<size_t>(id) < occupants.size()) {
        occupants[id].temperature = temp_c;
        return true;
    }
    return false;
}

// --- JSON Serialization & Export ---
std::string BuildingGeometry::export_to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    ss << "  \"version\": \"2.0\",\n";
    ss << "  \"domain\": { \"lx\": " << lx << ", \"ly\": " << ly << ", \"lz\": " << lz << " },\n";

    // Walls
    ss << "  \"walls\": [\n";
    for (size_t i = 0; i < walls.size(); ++i) {
        const auto& w = walls[i];
        ss << "    { \"id\": " << w.id << ", \"name\": \"" << w.name << "\", \"x1\": " << w.x1
           << ", \"z1\": " << w.z1 << ", \"x2\": " << w.x2 << ", \"z2\": " << w.z2
           << ", \"thickness\": " << w.thickness << ", \"height\": " << w.height
           << ", \"is_exterior\": " << (w.is_exterior ? "true" : "false") << " }"
           << (i + 1 < walls.size() ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Windows
    ss << "  \"windows\": [\n";
    for (size_t i = 0; i < windows.size(); ++i) {
        const auto& w = windows[i];
        ss << "    { \"id\": " << w.id << ", \"name\": \"" << w.name << "\", \"x\": " << w.x
           << ", \"y\": " << w.y << ", \"z\": " << w.z << ", \"width\": " << w.width
           << ", \"height\": " << w.height << ", \"normal\": [" << w.normal.x << "," << w.normal.y << "," << w.normal.z << "]"
           << ", \"open_ratio\": " << w.open_ratio << " }"
           << (i + 1 < windows.size() ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Doors
    ss << "  \"doors\": [\n";
    for (size_t i = 0; i < doors.size(); ++i) {
        const auto& d = doors[i];
        ss << "    { \"id\": " << d.id << ", \"name\": \"" << d.name << "\", \"x\": " << d.x
           << ", \"y\": " << d.y << ", \"z\": " << d.z << ", \"width\": " << d.width
           << ", \"height\": " << d.height << ", \"open_ratio\": " << d.open_ratio
           << ", \"is_connecting\": " << (d.is_connecting ? "true" : "false") << " }"
           << (i + 1 < doors.size() ? ",\n" : "\n");
    }
    ss << "  ],\n";

    // Occupants (6ft Dummies @ 37°C)
    ss << "  \"occupants\": [\n";
    for (size_t i = 0; i < occupants.size(); ++i) {
        const auto& o = occupants[i];
        ss << "    { \"id\": " << o.id << ", \"name\": \"" << o.name << "\", \"x\": " << o.x
           << ", \"z\": " << o.z << ", \"height\": " << o.height << ", \"width\": " << o.width
           << ", \"depth\": " << o.depth << ", \"temperature\": " << o.temperature
           << ", \"metabolic_rate\": " << o.metabolic_rate << " }"
           << (i + 1 < occupants.size() ? ",\n" : "\n");
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

static double extract_num(const std::string& str, const std::string& key, double def_val = 0.0) {
    size_t pos = str.find("\"" + key + "\"");
    if (pos == std::string::npos) return def_val;
    pos = str.find(':', pos);
    if (pos == std::string::npos) return def_val;
    pos++;
    while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '"')) pos++;
    try {
        return std::stod(str.substr(pos));
    } catch (...) {
        return def_val;
    }
}

bool BuildingGeometry::import_from_json(const std::string& json_str) {
    if (json_str.empty() || json_str.find("walls") == std::string::npos) return false;

    walls.clear();
    windows.clear();
    doors.clear();
    occupants.clear();

    next_wall_id = 1;
    next_win_id = 1;
    next_door_id = 1;
    next_occ_id = 1;

    // Parse walls
    size_t walls_pos = json_str.find("\"walls\"");
    size_t walls_end = json_str.find(']', walls_pos);
    if (walls_pos != std::string::npos && walls_end != std::string::npos) {
        std::string block = json_str.substr(walls_pos, walls_end - walls_pos);
        size_t cur = 0;
        while ((cur = block.find('{', cur)) != std::string::npos) {
            size_t end_item = block.find('}', cur);
            if (end_item == std::string::npos) break;
            std::string item = block.substr(cur, end_item - cur + 1);

            double x1 = extract_num(item, "x1");
            double z1 = extract_num(item, "z1");
            double x2 = extract_num(item, "x2");
            double z2 = extract_num(item, "z2");
            double th = extract_num(item, "thickness", 0.20);
            bool is_ext = (item.find("\"is_exterior\": true") != std::string::npos ||
                           item.find("\"is_exterior\":true") != std::string::npos);

            add_wall(x1, z1, x2, z2, th, is_ext);
            cur = end_item + 1;
        }
    }

    // Parse windows
    size_t win_pos = json_str.find("\"windows\"");
    size_t win_end = json_str.find(']', win_pos);
    if (win_pos != std::string::npos && win_end != std::string::npos) {
        std::string block = json_str.substr(win_pos, win_end - win_pos);
        size_t cur = 0;
        while ((cur = block.find('{', cur)) != std::string::npos) {
            size_t end_item = block.find('}', cur);
            if (end_item == std::string::npos) break;
            std::string item = block.substr(cur, end_item - cur + 1);

            double x = extract_num(item, "x");
            double y = extract_num(item, "y", 1.50);
            double z = extract_num(item, "z");
            double w = extract_num(item, "width", 1.50);
            double h = extract_num(item, "height", 1.20);
            double op = extract_num(item, "open_ratio", 1.0);

            Vec3 norm(1, 0, 0);
            if (x < 0.1) norm = Vec3(1, 0, 0);
            else if (x > lx - 0.1) norm = Vec3(-1, 0, 0);
            else if (z < 0.1) norm = Vec3(0, 0, 1);
            else norm = Vec3(0, 0, -1);

            add_window(x, y, z, w, h, norm, op);
            cur = end_item + 1;
        }
    }

    // Parse doors
    size_t dor_pos = json_str.find("\"doors\"");
    size_t dor_end = json_str.find(']', dor_pos);
    if (dor_pos != std::string::npos && dor_end != std::string::npos) {
        std::string block = json_str.substr(dor_pos, dor_end - dor_pos);
        size_t cur = 0;
        while ((cur = block.find('{', cur)) != std::string::npos) {
            size_t end_item = block.find('}', cur);
            if (end_item == std::string::npos) break;
            std::string item = block.substr(cur, end_item - cur + 1);

            double x = extract_num(item, "x");
            double z = extract_num(item, "z");
            double w = extract_num(item, "width", 1.00);
            double h = extract_num(item, "height", 2.10);
            double op = extract_num(item, "open_ratio", 1.0);
            bool is_conn = (item.find("\"is_connecting\": false") == std::string::npos &&
                            item.find("\"is_connecting\":false") == std::string::npos);

            add_door(x, z, w, h, Vec3(1, 0, 0), op, is_conn);
            cur = end_item + 1;
        }
    }

    // Parse occupants (6ft Dummies)
    size_t occ_pos = json_str.find("\"occupants\"");
    size_t occ_end = json_str.find(']', occ_pos);
    if (occ_pos != std::string::npos && occ_end != std::string::npos) {
        std::string block = json_str.substr(occ_pos, occ_end - occ_pos);
        size_t cur = 0;
        while ((cur = block.find('{', cur)) != std::string::npos) {
            size_t end_item = block.find('}', cur);
            if (end_item == std::string::npos) break;
            std::string item = block.substr(cur, end_item - cur + 1);

            double x = extract_num(item, "x", 2.5);
            double z = extract_num(item, "z", 2.5);
            double temp = extract_num(item, "temperature", 37.0);

            add_occupant(x, z, temp);
            cur = end_item + 1;
        }
    }

    return true;
}

// --- CFD Grid Voxelization ---
void BuildingGeometry::rasterize_to_grid(Grid3D& grid, const EnvironmentClimate& climate) {
    Vec3 wind_dir = climate.get_inflow_velocity(1.5);
    double wind_speed = wind_dir.norm();

    // 1. Initialize all cells to fluid
    for (int i = 0; i < grid.nx; ++i) {
        double x = grid.cell_x(i);
        bool at_x_bound = (i == 0 || i == grid.nx - 1);

        for (int j = 0; j < grid.ny; ++j) {
            double y = grid.cell_y(j);
            bool at_y_bound = (j == 0 || j == grid.ny - 1);

            for (int k = 0; k < grid.nz; ++k) {
                double z = grid.cell_z(k);
                bool at_z_bound = (k == 0 || k == grid.nz - 1);

                size_t c_idx = grid.idx_cell(i, j, k);
                grid.cell_type[c_idx] = CELL_FLUID;
                grid.heat_source[c_idx] = 0.0;

                // Domain boundary walls / ceiling / floor
                if (at_x_bound || at_y_bound || at_z_bound) {
                    grid.cell_type[c_idx] = CELL_SOLID_WALL;
                }

                // Check user-drawn walls
                for (const auto& w : walls) {
                    if (w.contains(x, y, z)) {
                        grid.cell_type[c_idx] = CELL_SOLID_WALL;
                        break;
                    }
                }

                // Overlay Windows (carve through solid walls)
                for (auto& win : windows) {
                    if (win.open_ratio > 0.05) {
                        AABB wbox = win.get_box();
                        if (wbox.contains(x, y, z)) {
                            // Compute if window is windward inflow or leeward outflow
                            double dot = win.normal.x * wind_dir.x + win.normal.z * wind_dir.z;
                            win.is_inflow = (dot > 0.0 || wind_speed < 0.05);
                            grid.cell_type[c_idx] = win.is_inflow ? CELL_WINDOW_INFLOW : CELL_WINDOW_OUTFLOW;
                        }
                    }
                }

                // Overlay Doors (carve doorway passage)
                for (const auto& dor : doors) {
                    if (dor.open_ratio > 0.05) {
                        AABB dbox = dor.get_box();
                        if (dbox.contains(x, y, z)) {
                            grid.cell_type[c_idx] = CELL_DOOR;
                        }
                    }
                }

                // Overlay 6-Foot Human Occupant Dummies @ 37°C
                for (const auto& occ : occupants) {
                    AABB obox = occ.get_box();
                    if (obox.contains(x, y, z)) {
                        grid.cell_type[c_idx] = CELL_HOT_BODY;
                        grid.T[c_idx] = occ.temperature;
                        grid.heat_source[c_idx] = occ.metabolic_rate / obox.volume();
                    }
                }
            }
        }
    }
}

// --- Boundary Conditions Enforcement ---
void BuildingGeometry::enforce_boundary_conditions(Grid3D& grid, const EnvironmentClimate& climate, double time) {
    // 1. Solid Walls - No-slip
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < grid.nx; ++i) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int k = 0; k < grid.nz; ++k) {
                if (grid.cell_type[grid.idx_cell(i, j, k)] == CELL_SOLID_WALL) {
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

    // 2. Windows - Inflow Wind Profile & Ambient Temperature
    double gust_factor = 1.0 + 0.10 * std::sin(0.4 * time) + 0.06 * std::cos(1.1 * time);

    for (const auto& w : windows) {
        if (w.open_ratio <= 0.01) continue;

        if (w.is_inflow) {
            AABB box = w.get_box();
            for (int i = 0; i < grid.nx; ++i) {
                double x = grid.cell_x(i);
                for (int j = 0; j < grid.ny; ++j) {
                    double y = grid.cell_y(j);
                    for (int k = 0; k < grid.nz; ++k) {
                        double z = grid.cell_z(k);
                        if (box.contains(x, y, z)) {
                            Vec3 inflow = climate.get_inflow_velocity(y);
                            double effective_speed = inflow.norm() * w.open_ratio * gust_factor;
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

    // 3. Human 6-foot Occupant Dummies @ 37 °C
    for (const auto& h : occupants) {
        AABB box = h.get_box();
        for (int i = 0; i < grid.nx; ++i) {
            double x = grid.cell_x(i);
            for (int j = 0; j < grid.ny; ++j) {
                double y = grid.cell_y(j);
                for (int k = 0; k < grid.nz; ++k) {
                    double z = grid.cell_z(k);
                    if (box.contains(x, y, z)) {
                        size_t c_idx = grid.idx_cell(i, j, k);
                        grid.T[c_idx] = h.temperature; // Strictly 37.0 °C
                        // No-slip velocity on dummy surface
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

} // namespace cfd
