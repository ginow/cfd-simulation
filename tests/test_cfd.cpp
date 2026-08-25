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
    std::cout << "[Test 2] Verifying 6-Foot Human Dummy (37°C) Thermal Buoyancy Plume... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 32; cfg.ny = 20; cfg.nz = 16;
    // Set calm wind to observe pure natural convective plume rising from 6ft 37°C dummy
    cfd::EnvironmentConfig env_cfg;
    env_cfg.wind_speed_ref = 0.0;
    env_cfg.outdoor_temp = 30.0;

    cfd::SimulationEngine engine(cfg, env_cfg);

    // Step natural convection
    engine.run_steps(50);

    // Check vertical velocity directly above Room 1 6ft occupant (height 1.83m, probe at y=2.1m)
    cfd::ProbeData probe = engine.query_probe(2.5, 2.1, 2.5);
    std::cout << "Plume Temperature: " << probe.temperature << " °C, Upward Velocity v: " << probe.v << " m/s ... ";

    assert(probe.temperature > 30.0 && "Temperature above 6ft occupant must be elevated above ambient 30°C");
    assert(probe.v > 0.0001 && "Upward buoyant thermal plume velocity must be positive (v > 0)");
    std::cout << "PASSED [✓]\n";
}

void test_cross_ventilation_continuity() {
    std::cout << "[Test 3] Verifying Cross-Ventilation Door Draft Continuity... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 40; cfg.ny = 18; cfg.nz = 20;
    cfd::EnvironmentConfig env_cfg;
    env_cfg.wind_speed_ref = 3.5;

    cfd::SimulationEngine engine(cfg, env_cfg);
    engine.run_steps(60);

    // Check velocity at the connecting door between Room 1 and Room 2 (at x=5.0, y=1.05, z=2.5)
    cfd::ProbeData door_probe = engine.query_probe(5.0, 1.05, 2.5);
    std::cout << "Doorway Flow Speed: " << door_probe.speed << " m/s (u=" << door_probe.u << ") ... ";

    assert(door_probe.speed > 0.05 && "Air must actively flow through the inter-room connecting door");
    std::cout << "PASSED [✓]\n";
}

void test_cad_dynamic_design_roundtrip() {
    std::cout << "[Test 4] Verifying Dynamic CAD Operations & JSON Export/Import... ";
    cfd::SimulationConfig cfg;
    cfd::SimulationEngine engine(cfg);

    // 1. Add custom wall
    int wall_id = engine.add_wall(2.0, 1.0, 2.0, 4.0, 0.20);
    assert(wall_id > 0 && "Wall ID must be positive");

    // 2. Add window
    int win_id = engine.add_window(2.0, 1.5, 2.5, 1.2, 1.0);
    assert(win_id > 0 && "Window ID must be positive");

    // 3. Export to JSON
    std::string json = engine.get_design_json();
    assert(json.find("\"walls\"") != std::string::npos && "JSON must contain walls");
    assert(json.find("\"windows\"") != std::string::npos && "JSON must contain windows");

    // 4. Import back
    bool imported = engine.load_design_json(json);
    assert(imported && "JSON import must succeed");

    std::cout << "JSON Export/Import Roundtrip ... PASSED [✓]\n";
}

void test_temperature_conservation() {
    std::cout << "[Test 5] Verifying Thermal Field Stability and Maximum/Minimum Principle... ";
    cfd::SimulationConfig cfg;
    cfg.nx = 32; cfg.ny = 16; cfg.nz = 16;
    cfd::SimulationEngine engine(cfg);
    engine.run_steps(40);

    double min_t = engine.stats.min_temperature;
    double max_t = engine.stats.max_temperature;
    std::cout << "T_min = " << min_t << " °C, T_max = " << max_t << " °C ... ";

    assert(min_t >= 29.5 && "Minimum temperature cannot drop unphysically below ambient");
    assert(max_t <= 37.5 && "Maximum temperature cannot exceed human hot body temperature (37°C)");
    std::cout << "PASSED [✓]\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  RUNNING CFD & CAD VERIFICATION & VALIDATION SUITE\n";
    std::cout << "========================================================\n";

    test_divergence_free();
    test_thermal_plume_buoyancy();
    test_cross_ventilation_continuity();
    test_cad_dynamic_design_roundtrip();
    test_temperature_conservation();

    std::cout << "========================================================\n";
    std::cout << "  ALL CFD & CAD TESTS PASSED SUCCESSFULLY! [✓]\n";
    std::cout << "========================================================\n";

    return 0;
}
