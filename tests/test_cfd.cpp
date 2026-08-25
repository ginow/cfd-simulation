#include "Common.hpp"
#include "SimulationEngine.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>

void test_divergence_free() {
    std::cout << "[Test 1] Verifying Divergence-Free Incompressible Projection... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 32; cfg.ny = 16; cfg.nz = 16;
    cfd::SimulationEngine engine(cfg);

    // Run 30 steps
    engine.run_steps(30);

    double max_div = engine.stats.max_divergence;
    std::cout << "Max Divergence = " << max_div << " s^-1 ... ";
    assert(max_div < 0.05 && "Divergence must remain bounded and strictly controlled by Poisson projection");
    std::cout << "PASSED [✓]\n";
}

void test_thermal_plume_buoyancy() {
    std::cout << "[Test 2] Verifying Boussinesq Thermal Buoyancy Plume Generation... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 32; cfg.ny = 20; cfg.nz = 16;
    // Set calm wind to observe pure natural convective plume rising from 37°C body
    cfd::ClimateConfig climate_cfg;
    climate_cfg.wind_speed_ref = 0.0;

    cfd::SimulationEngine engine(cfg, climate_cfg);

    // Step natural convection
    engine.run_steps(50);

    // Check vertical velocity directly above Room 1 occupant (at x=2.5, z=2.5, y=1.8m)
    cfd::ProbeData probe = engine.query_probe(2.5, 1.8, 2.5);
    std::cout << "Plume Temperature: " << probe.temperature << " °C, Upward Velocity v: " << probe.v << " m/s ... ";

    assert(probe.temperature > 32.0 && "Temperature above occupant must be elevated above ambient 32°C");
    assert(probe.v > 0.001 && "Upward buoyant thermal plume velocity must be positive (v > 0)");
    std::cout << "PASSED [✓]\n";
}

void test_cross_ventilation_continuity() {
    std::cout << "[Test 3] Verifying Cross-Ventilation Door Draft Continuity... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 40; cfg.ny = 18; cfg.nz = 20;
    cfd::ClimateConfig climate_cfg;
    climate_cfg.wind_speed_ref = 3.5; // Kerala breeze

    cfd::SimulationEngine engine(cfg, climate_cfg);
    engine.run_steps(60);

    // Check velocity at the connecting door between Room 1 and Room 2 (at x=5.0, y=1.2, z=2.5)
    cfd::ProbeData door_probe = engine.query_probe(5.0, 1.2, 2.5);
    std::cout << "Doorway Flow Speed: " << door_probe.speed << " m/s (u=" << door_probe.u << ") ... ";

    assert(door_probe.speed > 0.05 && "Air must actively flow through the inter-room connecting door");
    std::cout << "PASSED [✓]\n";
}

void test_temperature_conservation() {
    std::cout << "[Test 4] Verifying Thermal Field Stability and Maximum/Minimum Principle... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 32; cfg.ny = 16; cfg.nz = 16;
    cfd::SimulationEngine engine(cfg);
    engine.run_steps(40);

    double min_t = engine.stats.min_temperature;
    double max_t = engine.stats.max_temperature;
    std::cout << "T_min = " << min_t << " °C, T_max = " << max_t << " °C ... ";

    assert(min_t >= 31.5 && "Minimum temperature cannot drop unphysically below ambient");
    assert(max_t <= 37.5 && "Maximum temperature cannot exceed human hot body temperature (37°C)");
    std::cout << "PASSED [✓]\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  RUNNING CFD VERIFICATION & VALIDATION SUITE\n";
    std::cout << "========================================================\n";

    test_divergence_free();
    test_thermal_plume_buoyancy();
    test_cross_ventilation_continuity();
    test_temperature_conservation();

    std::cout << "========================================================\n";
    std::cout << "  ALL CFD VERIFICATION TESTS PASSED SUCCESSFULLY! [✓]\n";
    std::cout << "========================================================\n";

    return 0;
}
