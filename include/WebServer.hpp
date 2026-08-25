#pragma once

#include "Common.hpp"
#include "SimulationEngine.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace cfd {

class WebServer {
public:
    int port;
    std::string web_root;
    SimulationEngine& engine;
    std::atomic<bool> is_running{false};
    std::unique_ptr<std::thread> server_thread;

    WebServer(int server_port, const std::string& static_dir, SimulationEngine& sim_engine);
    ~WebServer();

    bool start();
    void stop();

private:
    void run_server_loop(int server_fd);
    void handle_client(int client_fd);
    
    std::string handle_http_request(const std::string& request_str, int client_fd);
    std::string get_mime_type(const std::string& path);
    std::string read_file_contents(const std::string& filepath);
};

} // namespace cfd
