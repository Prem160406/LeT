#ifndef HTTPLIB_SINGLE_H
#define HTTPLIB_SINGLE_H

/**
 * ============================================================================
 * EXCEPTION NOTICE: HTTP TRANSPORT & JSON SERIALIZATION LAYER ONLY
 * ============================================================================
 * As approved under user requirements amendment #2:
 * "Use a single-header, dependency-free HTTP library for the transport layer only.
 *  All 8 core data structures remain 100% custom/from-scratch as originally specified.
 *  This exception applies ONLY to HTTP request handling and JSON serialization,
 *  not to any DSA logic."
 * 
 * In accordance with amendment #3:
 * Requests are handled in a single-threaded, synchronous loop on WinSock2
 * to prevent data-race hazards across the custom in-memory data structures.
 * ============================================================================
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <functional>
#include <map>
#include <vector>

namespace httplib_single {

struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> params;

    std::string get_param_value(const std::string& key) const {
        auto it = params.find(key);
        if (it != params.end()) return it->second;
        return "";
    }
};

struct Response {
    int status;
    std::string content_type;
    std::string body;

    Response() : status(200), content_type("text/plain"), body("") {}

    void set_content(const std::string& b, const std::string& ct) {
        body = b;
        content_type = ct;
    }
};

class SingleThreadedServer {
public:
    using Handler = std::function<void(const Request&, Response&)>;

private:
    std::map<std::string, Handler> get_routes;
    std::map<std::string, Handler> post_routes;
    std::string mount_dir;
    SOCKET listen_sock;
    bool is_running;

    std::string get_mime_type(const std::string& path) {
        if (path.find(".html") != std::string::npos) return "text/html";
        if (path.find(".css") != std::string::npos) return "text/css";
        if (path.find(".js") != std::string::npos) return "application/javascript";
        if (path.find(".json") != std::string::npos) return "application/json";
        return "text/plain";
    }

    void parse_query(const std::string& query, std::map<std::string, std::string>& params) {
        std::istringstream stream(query);
        std::string pair;
        while (std::getline(stream, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                params[pair.substr(0, eq)] = pair.substr(eq + 1);
            }
        }
    }

public:
    SingleThreadedServer() : mount_dir("./frontend"), listen_sock(INVALID_SOCKET), is_running(false) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~SingleThreadedServer() {
        stop();
        WSACleanup();
    }

    void Get(const std::string& pattern, Handler handler) {
        get_routes[pattern] = handler;
    }

    void Post(const std::string& pattern, Handler handler) {
        post_routes[pattern] = handler;
    }

    void set_mount_point(const std::string&, const std::string& dir) {
        mount_dir = dir;
    }

    void stop() {
        if (is_running) {
            is_running = false;
            if (listen_sock != INVALID_SOCKET) {
                closesocket(listen_sock);
                listen_sock = INVALID_SOCKET;
            }
        }
    }

    bool listen(const std::string& host, int port) {
        listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock == INVALID_SOCKET) {
            std::cerr << "Failed to create socket." << std::endl;
            return false;
        }

        BOOL opt = TRUE;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(host.c_str());
        server_addr.sin_port = htons(port);

        if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed on port " << port << std::endl;
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
            return false;
        }

        if (::listen(listen_sock, 10) == SOCKET_ERROR) {
            std::cerr << "Listen failed." << std::endl;
            closesocket(listen_sock);
            listen_sock = INVALID_SOCKET;
            return false;
        }

        is_running = true;
        std::cout << "\n========================================================" << std::endl;
        std::cout << "  Lab Operations Engine Web Server Live!" << std::endl;
        std::cout << "  Access Web UI at: http://" << host << ":" << port << "/" << std::endl;
        std::cout << "  (Single-threaded synchronous mode - Zero Race Hazards)" << std::endl;
        std::cout << "  Press Ctrl+C to terminate server." << std::endl;
        std::cout << "========================================================\n" << std::endl;

        // Synchronous single-threaded accept loop
        while (is_running) {
            sockaddr_in client_addr;
            int client_len = sizeof(client_addr);
            SOCKET client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
            if (client_sock == INVALID_SOCKET) {
                if (!is_running) break;
                continue;
            }

            std::string raw_req = "";
            char buffer[8192];
            int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                raw_req.append(buffer, bytes_read);

                // Check for Content-Length in headers
                size_t cl_pos = raw_req.find("Content-Length:");
                if (cl_pos == std::string::npos) {
                    cl_pos = raw_req.find("content-length:");
                }
                size_t content_length = 0;
                if (cl_pos != std::string::npos) {
                    size_t val_start = raw_req.find_first_of("0123456789", cl_pos);
                    if (val_start != std::string::npos) {
                        size_t val_end = raw_req.find_first_not_of("0123456789", val_start);
                        content_length = std::stoul(raw_req.substr(val_start, val_end - val_start));
                    }
                }

                // If body not fully received, read remainder
                size_t body_pos = raw_req.find("\r\n\r\n");
                while (body_pos != std::string::npos && content_length > 0) {
                    size_t curr_body_len = raw_req.size() - (body_pos + 4);
                    if (curr_body_len >= content_length) break;
                    int more = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                    if (more <= 0) break;
                    raw_req.append(buffer, more);
                }

                // Parse HTTP Header
                std::istringstream stream(raw_req);
                std::string method, full_path, version;
                stream >> method >> full_path >> version;

                std::string path = full_path;
                std::string query = "";
                size_t q_pos = full_path.find('?');
                if (q_pos != std::string::npos) {
                    path = full_path.substr(0, q_pos);
                    query = full_path.substr(q_pos + 1);
                }

                // Extract body if POST
                std::string body = "";
                body_pos = raw_req.find("\r\n\r\n");
                if (body_pos != std::string::npos) {
                    body = raw_req.substr(body_pos + 4);
                }

                Request req;
                req.method = method;
                req.path = path;
                req.query = query;
                req.body = body;
                parse_query(query, req.params);

                Response res;

                // Route matching
                bool handled = false;
                if (method == "GET") {
                    auto it = get_routes.find(path);
                    if (it != get_routes.end()) {
                        it->second(req, res);
                        handled = true;
                    } else {
                        // Static file handling
                        std::string rel_path = path;
                        if (rel_path == "/") rel_path = "/index.html";
                        std::string file_path = mount_dir + rel_path;
                        std::ifstream file(file_path.c_str(), std::ios::binary);
                        if (file.is_open()) {
                            std::ostringstream ss;
                            ss << file.rdbuf();
                            res.set_content(ss.str(), get_mime_type(rel_path));
                            handled = true;
                        }
                    }
                } else if (method == "POST") {
                    auto it = post_routes.find(path);
                    if (it != post_routes.end()) {
                        it->second(req, res);
                        handled = true;
                    }
                } else if (method == "OPTIONS") {
                    res.status = 200;
                    res.set_content("", "text/plain");
                    handled = true;
                }

                if (!handled) {
                    res.status = 404;
                    res.set_content("{\"error\":\"Not Found\"}", "application/json");
                }

                // Format HTTP Response
                std::ostringstream resp_stream;
                resp_stream << "HTTP/1.1 " << res.status << " OK\r\n";
                resp_stream << "Content-Type: " << res.content_type << "\r\n";
                resp_stream << "Content-Length: " << res.body.size() << "\r\n";
                resp_stream << "Access-Control-Allow-Origin: *\r\n";
                resp_stream << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
                resp_stream << "Access-Control-Allow-Headers: Content-Type\r\n";
                resp_stream << "Connection: close\r\n\r\n";
                resp_stream << res.body;

                std::string resp_str = resp_stream.str();
                send(client_sock, resp_str.c_str(), static_cast<int>(resp_str.size()), 0);
                shutdown(client_sock, SD_SEND);
            }

            closesocket(client_sock);
        }

        return true;
    }
};

} // namespace httplib_single

#endif // HTTPLIB_SINGLE_H
