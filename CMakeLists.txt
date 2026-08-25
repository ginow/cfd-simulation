cmake_minimum_required(VERSION 3.16)
project(CFDSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

include_directories(include)

set(SOURCES
    src/Grid3D.cpp
    src/Geometry.cpp
    src/KeralaClimate.cpp
    src/Turbulence.cpp
    src/PressurePoisson.cpp
    src/NavierStokesSolver.cpp
    src/ThermalSolver.cpp
    src/VTKExporter.cpp
    src/SimulationEngine.cpp
    src/WebServer.cpp
)

add_executable(cfd_sim src/main.cpp ${SOURCES})
target_link_libraries(cfd_sim PRIVATE OpenMP::OpenMP_CXX Threads::Threads)
target_compile_options(cfd_sim PRIVATE -O3 -Wall -Wextra)

add_executable(test_cfd tests/test_cfd.cpp ${SOURCES})
target_link_libraries(test_cfd PRIVATE OpenMP::OpenMP_CXX Threads::Threads)
target_compile_options(test_cfd PRIVATE -O3 -Wall -Wextra)
