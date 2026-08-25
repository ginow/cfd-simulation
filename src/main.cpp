#include "Common.hpp"
#include "SimulationEngine.hpp"
#include "WebServer.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <omp.h>

static std::atomic<bool> g_keep_running{true};

void signal_handler([[maybe_unused]] int signal) {
    g_keep_running.store(false);
}

void print_banner() {
    std::cout << "======================================================================\n";
    std::cout << "  AEROTHERMAL3D: COUPLED THERMAL-FLUID CFD INDOOR CONVECTION ENGINE\n";
    std::cout << "  Large Eddy Simulation (LES) & Boussinesq Thermal Convection\n";
    std::cout << "  Universal Atmospheric Boundary Layer & Interactive CAD Architecture\n";
    std::cout << "======================================================================\n";
    std::cout << " [✓] Physical Domain: 10.0 m (L) x 2.80 m (H) x 5.0 m (W) [Fixed 280cm Ceiling]\n";
    std::cout << " [✓] Environment    : Parametric ABL Profile (Wind Speed, Angle, Outdoor Temp)\n";
    std::cout << " [✓] Human Dummy    : 6-Foot Occupants (1.83m) @ 37.0°C (Buoyant Thermal Plumes)\n";
    std::cout << " [✓] Architecture   : Dynamic CAD Walls, Adjustable Casement Windows & Doors\n";
    std::cout << " [✓] Optimizations  : SIMD AVX2 + FMA + Cache-Contiguous Memory + Fast SOR\n";
    std::cout << " [✓] OpenMP Threads : " << omp_get_max_threads() << " threads detected and enabled\n";
    std::cout << "======================================================================\n\n";
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGHUP, SIG_IGN);

    int port = 8080;
    bool headless = false;
    int headless_steps = 300;
    int substeps = 4;
    std::string vtk_output = "cfd_room_output.vtk";
    int num_threads = omp_get_max_threads();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--headless") {
            headless = true;
        } else if (arg == "--substeps" && i + 1 < argc) {
            substeps = std::stoi(argv[++i]);
        } else if (arg == "--steps" && i + 1 < argc) {
            headless_steps = std::stoi(argv[++i]);
        } else if (arg == "--vtk" && i + 1 < argc) {
            vtk_output = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
            omp_set_num_threads(num_threads);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  --port <N>      Web visualization port (default: 8080)\n"
                      << "  --substeps <N>  CFD physics sub-steps per frame (default: 4 for real-time speed)\n"
                      << "  --headless      Run headless benchmark without GUI\n"
                      << "  --steps <N>     Steps to run in headless mode (default: 300)\n"
                      << "  --vtk <file>    Export final VTK state file (default: cfd_room_output.vtk)\n"
                      << "  --threads <N>   Number of OpenMP threads to use\n"
                      << "  --help, -h      Show this help message\n";
            return 0;
        }
    }

    omp_set_num_threads(num_threads);
    print_banner();

    cfd::SimulationConfig config;
    cfd::ClimateConfig climate_cfg;

    cfd::SimulationEngine engine(config, climate_cfg);
    engine.set_substeps(substeps);

    // 1. Headless Batch Mode
    if (headless) {
        std::cout << "[Headless Mode] Running " << headless_steps << " CFD timesteps with AVX2 acceleration...\n";
        auto t_start = std::chrono::high_resolution_clock::now();

        for (int s = 1; s <= headless_steps && g_keep_running.load(); ++s) {
            engine.step();
            if (s % 50 == 0 || s == headless_steps) {
                std::cout << "  Step " << s << "/" << headless_steps
                          << " | Time: " << engine.stats.current_time << " s"
                          << " | dt: " << engine.stats.current_dt << " s"
                          << " | Max Vel: " << engine.stats.max_velocity << " m/s"
                          << " | Avg Temp: " << engine.stats.avg_temperature << " °C"
                          << " | Max Temp: " << engine.stats.max_temperature << " °C"
                          << " | Divergence: " << engine.stats.max_divergence
                          << " | Compute: " << engine.stats.computation_time_ms << " ms\n";
            }
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        std::cout << "[Headless Mode] Completed " << headless_steps << " steps in "
                  << total_ms << " ms (" << (headless_steps / (total_ms / 1000.0)) << " steps/sec)\n";

        std::cout << "[Headless Mode] Exporting VTK file to " << vtk_output << " ...\n";
        if (engine.export_vtk(vtk_output)) {
            std::cout << "[Headless Mode] VTK export successful! Open with ParaView.\n";
        }
        return 0;
    }

    // 2. Interactive Web Dashboard Visualizer Mode
    std::string web_root = "web";
    std::ifstream test_f("web/index.html");
    if (!test_f.good()) {
        web_root = "/mnt/new_volume/source/CFDSimulation/web";
    }

    cfd::WebServer server(port, web_root, engine);
    if (!server.start()) {
        std::cerr << "Fatal: Failed to start web visualizer server on port " << port << std::endl;
        return 1;
    }

    std::cout << ">>> Open your browser and navigate to: http://localhost:" << server.port << " <<<\n";
    std::cout << ">>> Sub-stepping: " << substeps << " CFD steps per frame (Real-Time 1.0x Physical Speed) <<<\n";
    std::cout << ">>> Press Ctrl+C to terminate simulation engine <<<\n\n";

    // Main physics simulation background loop with multi-substepping
    while (g_keep_running.load()) {
        if (!engine.is_paused.load()) {
            engine.step_multi(engine.sub_steps);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    std::cout << "\n[Engine] Shutting down simulation and web server..." << std::endl;
    server.stop();
    std::cout << "[Engine] Clean shutdown complete." << std::endl;

    return 0;
}
