#pragma once

#include "Common.hpp"
#include <cmath>

namespace cfd {

class EnvironmentClimate {
public:
    EnvironmentConfig config;

    EnvironmentClimate(const EnvironmentConfig& cfg = EnvironmentConfig()) : config(cfg) {}

    // Atmospheric Boundary Layer (ABL) horizontal wind speed at height y [m] via power-law shear model
    double get_abl_speed(double y) const {
        double y_eff = std::max(y, 0.1);
        return config.wind_speed_ref * std::pow(y_eff / config.z_ref, config.terrain_alpha);
    }

    // Horizontal wind vector (u_ext, 0, w_ext) entering through exterior openings
    Vec3 get_inflow_velocity(double y) const {
        double speed = get_abl_speed(y);
        double rad = config.wind_angle_deg * M_PI / 180.0;
        // Direction vector for incoming wind
        double u_comp = speed * std::cos(rad);
        double w_comp = speed * std::sin(rad);
        return Vec3(u_comp, 0.0, w_comp);
    }

    // Ambient outdoor temperature at height y
    double get_outdoor_temperature([[maybe_unused]] double y) const {
        return config.outdoor_temp;
    }

    // Exterior surface wall temperature accounting for optional solar radiation
    double get_exterior_wall_temperature(bool sun_facing) const {
        if (sun_facing) {
            return config.outdoor_temp + (config.solar_heat_flux / 75.0);
        }
        return config.outdoor_temp;
    }
};

using KeralaClimate = EnvironmentClimate; // Alias for backward compatibility

} // namespace cfd
