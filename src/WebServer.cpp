#include "WebServer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <vector>
#include <algorithm>

namespace cfd {

WebServer::WebServer(int p, const std::string& root, SimulationEngine& eng)
    : port(p), web_root(root), engine(eng) {}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    int server_fd = -1;
    int try_port = port;
    const int max_port_tries = 10;

    for (int i = 0; i < max_port_tries; ++i) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "[WebServer] Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(try_port);

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            port = try_port;
            break;
        }

        close(server_fd);
        server_fd = -1;
        try_port++;
    }

    if (server_fd < 0) {
        std::cerr << "[WebServer] Failed to bind to port range starting at " << port << std::endl;
        return false;
    }

    if (listen(server_fd, 32) < 0) {
        std::cerr << "[WebServer] Failed to listen on port " << port << std::endl;
        close(server_fd);
        return false;
    }

    is_running.store(true);
    server_thread = std::make_unique<std::thread>(&WebServer::run_server_loop, this, server_fd);

    std::cout << "[WebServer] CFD Simulation Visualizer running on http://localhost:" << port << std::endl;
    return true;
}

void WebServer::stop() {
    if (is_running.load()) {
        is_running.store(false);
        if (server_thread && server_thread->joinable()) {
            server_thread->join();
        }
    }
}

void WebServer::run_server_loop(int server_fd) {
    std::cout << "[WebServer] Server loop started on fd " << server_fd << " (port " << port << ")" << std::endl;

    while (is_running.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms

        int activity = select(server_fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (activity < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[WebServer] select error: " << strerror(errno) << std::endl;
            break;
        }

        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                std::thread([this, client_fd]() {
                    handle_client(client_fd);
                }).detach();
            }
        }
    }

    std::cout << "[WebServer] Server loop exiting for fd " << server_fd << std::endl;
    close(server_fd);
}

void WebServer::handle_client(int client_fd) {
    std::string request_str;
    char buffer[4096];
    size_t content_length = 0;
    bool headers_done = false;

    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break;
        buffer[bytes_read] = '\0';
        request_str.append(buffer, bytes_read);

        if (!headers_done) {
            size_t header_end = request_str.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                headers_done = true;
                // Parse Content-Length if present
                size_t cl_pos = request_str.find("Content-Length:");
                if (cl_pos == std::string::npos) cl_pos = request_str.find("content-length:");
                if (cl_pos != std::string::npos && cl_pos < header_end) {
                    size_t val_start = cl_pos + 15;
                    while (val_start < header_end && request_str[val_start] == ' ') val_start++;
                    try {
                        content_length = std::stoul(request_str.substr(val_start));
                    } catch (...) {
                        content_length = 0;
                    }
                }
            }
        }

        if (headers_done) {
            size_t header_end = request_str.find("\r\n\r\n");
            size_t body_len = request_str.size() - (header_end + 4);
            if (body_len >= content_length) {
                break;
            }
        }
    }

    if (request_str.empty()) {
        close(client_fd);
        return;
    }

    std::string response = handle_http_request(request_str, client_fd);
    if (!response.empty()) {
        size_t total_sent = 0;
        while (total_sent < response.size()) {
            ssize_t sent = send(client_fd, response.c_str() + total_sent, response.size() - total_sent, MSG_NOSIGNAL);
            if (sent <= 0) break;
            total_sent += sent;
        }
    }
    close(client_fd);
}

static std::string get_query_param(const std::string& query, const std::string& key) {
    size_t pos = query.find(key + "=");
    if (pos == std::string::npos) return "";
    pos += key.size() + 1;
    size_t end_pos = query.find('&', pos);
    if (end_pos == std::string::npos) {
        return query.substr(pos);
    }
    return query.substr(pos, end_pos - pos);
}

static std::string get_json_string(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.length());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - (pos + 1));
}

static double get_json_number(const std::string& json, const std::string& key, double default_val = 0.0) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;
    pos = json.find(':', pos + needle.length());
    if (pos == std::string::npos) return default_val;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) pos++;
    try {
        return std::stod(json.substr(pos));
    } catch (...) {
        return default_val;
    }
}

static int get_json_int(const std::string& json, const std::string& key, int default_val = 0) {
    return static_cast<int>(get_json_number(json, key, default_val));
}

std::string WebServer::get_mime_type(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".json")) return "application/json";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".svg"))  return "image/svg+xml";
    if (path.ends_with(".ico"))  return "image/x-icon";
    return "text/plain";
}

std::string WebServer::read_file_contents(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string WebServer::handle_http_request(const std::string& request_str, [[maybe_unused]] int client_fd) {
    std::istringstream stream(request_str);
    std::string method, url, version;
    stream >> method >> url >> version;

    // Parse URL and Query parameters
    std::string path = url;
    std::string query_str;
    size_t q_pos = url.find('?');
    if (q_pos != std::string::npos) {
        path = url.substr(0, q_pos);
        query_str = url.substr(q_pos + 1);
    }

    // CORS Headers
    std::string cors_headers = "Access-Control-Allow-Origin: *\r\n"
                               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                               "Access-Control-Allow-Headers: Content-Type\r\n";

    if (method == "OPTIONS") {
        return "HTTP/1.1 204 No Content\r\n" + cors_headers + "Content-Length: 0\r\n\r\n";
    }

    // 1. API Endpoints
    if (path == "/api/stats") {
        std::string json = engine.get_stats_json();
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    if (path == "/api/slice") {
        int axis = 1; // Default Y-slice
        int index = 14;
        std::string axis_str = get_query_param(query_str, "axis");
        std::string index_str = get_query_param(query_str, "index");

        if (!axis_str.empty()) axis = std::stoi(axis_str);
        if (!index_str.empty()) index = std::stoi(index_str);

        std::string json = engine.get_slice_json(axis, index);
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    if (path == "/api/3d") {
        std::string json = engine.get_3d_state_json();
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    if (path == "/api/probe") {
        double x = 2.5, y = 1.5, z = 2.5;
        std::string xs = get_query_param(query_str, "x");
        std::string ys = get_query_param(query_str, "y");
        std::string zs = get_query_param(query_str, "z");
        if (!xs.empty()) x = std::stod(xs);
        if (!ys.empty()) y = std::stod(ys);
        if (!zs.empty()) z = std::stod(zs);

        std::string json = engine.get_probe_json(x, y, z);
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    if (path == "/api/action" && method == "POST") {
        size_t body_pos = request_str.find("\r\n\r\n");
        std::string body = (body_pos != std::string::npos) ? request_str.substr(body_pos + 4) : "";
        std::string cmd = get_json_string(body, "cmd");

        // Execute only on exact command match
        if (cmd == "toggle_pause") {
            engine.set_paused(!engine.is_paused.load());
        } else if (cmd == "step") {
            engine.step();
        } else if (cmd == "reset") {
            engine.reset();
        } else if (cmd == "set_wind_speed") {
            engine.set_wind_speed(get_json_number(body, "val", 3.5));
        } else if (cmd == "set_wind_angle") {
            engine.set_wind_angle(get_json_number(body, "val", 225.0));
        } else if (cmd == "set_outdoor_temp") {
            engine.set_outdoor_temp(get_json_number(body, "val", 32.0));
        } else if (cmd == "set_human_temp") {
            int r = get_json_int(body, "room", 0);
            double t = get_json_number(body, "val", 37.0);
            engine.set_human_temp(r, t);
        } else if (cmd == "set_window") {
            int idx = get_json_int(body, "idx", 0);
            double op = get_json_number(body, "open", 1.0);
            engine.set_window_state(idx, op);
        } else if (cmd == "set_door") {
            int idx = get_json_int(body, "idx", 0);
            double op = get_json_number(body, "open", 1.0);
            engine.set_door_state(idx, op);
        } else if (cmd == "move_window") {
            int idx = get_json_int(body, "idx", 0);
            double x = get_json_number(body, "x", 0.0);
            double z = get_json_number(body, "z", 0.0);
            engine.move_window(idx, x, z);
        } else if (cmd == "move_door") {
            int idx = get_json_int(body, "idx", 0);
            double x = get_json_number(body, "x", 0.0);
            double z = get_json_number(body, "z", 0.0);
            engine.move_door(idx, x, z);
        } else if (cmd == "set_substeps") {
            int s = get_json_int(body, "val", 4);
            engine.set_substeps(s);
        } else if (cmd == "export_vtk") {
            engine.export_vtk("indoor_cfd_thermal.vtk");
        }

        std::string json = "{\"status\":\"ok\"}";
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    // 2. Static File Serving
    if (path == "/") path = "/index.html";
    std::string filepath = web_root + path;
    std::string content = read_file_contents(filepath);

    if (!content.empty()) {
        std::string mime = get_mime_type(filepath);
        return "HTTP/1.1 200 OK\r\n" + cors_headers +
               "Content-Type: " + mime + "\r\n" +
               "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
    }

    // 404 Not Found
    std::string not_found = "404 Not Found";
    return "HTTP/1.1 404 Not Found\r\n" + cors_headers +
           "Content-Type: text/plain\r\n" +
           "Content-Length: " + std::to_string(not_found.size()) + "\r\n\r\n" + not_found;
}

} // namespace cfd
