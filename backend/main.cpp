#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <climits>
#include <sstream>
#include <cctype>

// Step execution details
struct Step {
    int page;
    bool is_hit;
    std::vector<int> frames;
    int replaced; // -1 if no eviction
};

// Summary metrics structure
struct SimulationResult {
    std::string algorithm;
    int capacity;
    int hits;
    int faults;
    double hit_ratio;
    double fault_ratio;
    std::vector<Step> history;
};

// Helper function to build metrics
SimulationResult build_result(const std::string& algo, int capacity, int hits, int faults, const std::vector<Step>& history) {
    double total = hits + faults;
    double hit_ratio = total > 0 ? (double)hits / total : 0.0;
    double fault_ratio = total > 0 ? (double)faults / total : 0.0;
    return {algo, capacity, hits, faults, hit_ratio, fault_ratio, history};
}

// 1. First-In, First-Out (FIFO)
SimulationResult run_fifo(const std::vector<int>& ref_str, int capacity) {
    std::vector<int> frames;
    std::vector<Step> history;
    int hits = 0, faults = 0;
    int fifo_pointer = 0;

    for (int page : ref_str) {
        auto it = std::find(frames.begin(), frames.end(), page);
        bool is_hit = (it != frames.end());
        int replaced = -1;

        if (is_hit) {
            hits++;
        } else {
            faults++;
            if ((int)frames.size() < capacity) {
                frames.push_back(page);
            } else {
                replaced = frames[fifo_pointer];
                frames[fifo_pointer] = page;
                fifo_pointer = (fifo_pointer + 1) % capacity;
            }
        }
        history.push_back({page, is_hit, frames, replaced});
    }
    return build_result("FIFO", capacity, hits, faults, history);
}

// 2. Least Recently Used (LRU)
SimulationResult run_lru(const std::vector<int>& ref_str, int capacity) {
    std::vector<int> frames;
    std::unordered_map<int, int> last_used;
    std::vector<Step> history;
    int hits = 0, faults = 0;

    for (int i = 0; i < (int)ref_str.size(); i++) {
        int page = ref_str[i];
        auto it = std::find(frames.begin(), frames.end(), page);
        bool is_hit = (it != frames.end());
        int replaced = -1;

        if (is_hit) {
            hits++;
        } else {
            faults++;
            if ((int)frames.size() < capacity) {
                frames.push_back(page);
            } else {
                int lru_index = 0;
                int min_time = INT_MAX;
                for (int j = 0; j < (int)frames.size(); j++) {
                    if (last_used[frames[j]] < min_time) {
                        min_time = last_used[frames[j]];
                        lru_index = j;
                    }
                }
                replaced = frames[lru_index];
                frames[lru_index] = page;
            }
        }
        last_used[page] = i;
        history.push_back({page, is_hit, frames, replaced});
    }
    return build_result("LRU", capacity, hits, faults, history);
}

// 3. Optimal Page Replacement (Belady's)
SimulationResult run_optimal(const std::vector<int>& ref_str, int capacity) {
    std::vector<int> frames;
    std::vector<Step> history;
    int hits = 0, faults = 0;

    for (int i = 0; i < (int)ref_str.size(); i++) {
        int page = ref_str[i];
        auto it = std::find(frames.begin(), frames.end(), page);
        bool is_hit = (it != frames.end());
        int replaced = -1;

        if (is_hit) {
            hits++;
        } else {
            faults++;
            if ((int)frames.size() < capacity) {
                frames.push_back(page);
            } else {
                int victim_index = -1;
                int farthest_use = -1;

                for (int j = 0; j < (int)frames.size(); j++) {
                    int next_use = INT_MAX;
                    for (int k = i + 1; k < (int)ref_str.size(); k++) {
                        if (ref_str[k] == frames[j]) {
                            next_use = k;
                            break;
                        }
                    }
                    if (next_use > farthest_use) {
                        farthest_use = next_use;
                        victim_index = j;
                    }
                }
                replaced = frames[victim_index];
                frames[victim_index] = page;
            }
        }
        history.push_back({page, is_hit, frames, replaced});
    }
    return build_result("OPTIMAL", capacity, hits, faults, history);
}

// Convert SimulationResult struct to JSON string
std::string result_to_json(const SimulationResult& res) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"algorithm\":\"" << res.algorithm << "\",";
    ss << "\"capacity\":" << res.capacity << ",";
    ss << "\"hits\":" << res.hits << ",";
    ss << "\"faults\":" << res.faults << ",";
    ss << "\"hit_ratio\":" << res.hit_ratio << ",";
    ss << "\"fault_ratio\":" << res.fault_ratio << ",";
    ss << "\"history\":[";

    for (size_t i = 0; i < res.history.size(); ++i) {
        const auto& step = res.history[i];
        ss << "{";
        ss << "\"page\":" << step.page << ",";
        ss << "\"is_hit\":" << (step.is_hit ? "true" : "false") << ",";
        ss << "\"replaced\":" << step.replaced << ",";
        ss << "\"frames\":[";
        for (size_t f = 0; f < step.frames.size(); ++f) {
            ss << step.frames[f] << (f + 1 < step.frames.size() ? "," : "");
        }
        ss << "]}";
        if (i + 1 < res.history.size()) ss << ",";
    }

    ss << "]}";
    return ss.str();
}

// JSON parsing helper functions
int parse_int_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    size_t start = json.find_first_of("0123456789", pos);
    if (start == std::string::npos) return 0;
    return std::stoi(json.substr(start));
}

std::string parse_string_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    size_t start = json.find("\"", pos);
    if (start == std::string::npos) return "";
    size_t end = json.find("\"", start + 1);
    if (end == std::string::npos) return "";
    return json.substr(start + 1, end - start - 1);
}

std::vector<int> parse_array_field(const std::string& json, const std::string& key) {
    std::vector<int> result;
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    size_t start = json.find("[", pos);
    size_t end = json.find("]", start);
    if (start == std::string::npos || end == std::string::npos) return result;

    std::string arr_str = json.substr(start + 1, end - start - 1);
    std::string cur = "";
    for (char c : arr_str) {
        if (isdigit(c)) {
            cur += c;
        } else if (!cur.empty()) {
            result.push_back(std::stoi(cur));
            cur = "";
        }
    }
    if (!cur.empty()) {
        result.push_back(std::stoi(cur));
    }
    return result;
}

// Helper to construct HTTP response with CORS headers
std::string make_http_response(const std::string& body_json, int status_code = 200) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status_code << " OK\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Headers: Content-Type\r\n"
       << "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
       << "Content-Type: application/json\r\n"
       << "Content-Length: " << body_json.length() << "\r\n\r\n"
       << body_json;
    return ss.str();
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed on port 8080.\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << " OS Virtual Memory Engine running at http://localhost:8080\n";
    std::cout << " Supported Algorithms: FIFO, LRU, OPTIMAL\n";
    std::cout << "========================================================\n";

    while (true) {
        SOCKET client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == INVALID_SOCKET) continue;

        char buffer[8192] = {0};
        int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0) {
            std::string req(buffer, bytes_read);

            // CORS OPTIONS preflight handler
            if (req.find("OPTIONS") == 0) {
                std::string res = "HTTP/1.1 200 OK\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Access-Control-Allow-Headers: Content-Type\r\n"
                                  "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                                  "Content-Length: 0\r\n\r\n";
                send(client_fd, res.c_str(), (int)res.length(), 0);
            } 
            // POST endpoints (/api/simulate and /api/compare)
            else if (req.find("POST /api/simulate") != std::string::npos || req.find("POST /api/compare") != std::string::npos) {
                size_t body_pos = req.find("\r\n\r\n");
                std::string body_str = (body_pos != std::string::npos) ? req.substr(body_pos + 4) : "";

                std::string http_res;
                try {
                    int capacity = parse_int_field(body_str, "frames_count");
                    std::vector<int> ref_str = parse_array_field(body_str, "reference_string");

                    if (req.find("/api/simulate") != std::string::npos) {
                        std::string algo = parse_string_field(body_str, "algorithm");
                        SimulationResult res;

                        if (algo == "FIFO") res = run_fifo(ref_str, capacity);
                        else if (algo == "LRU") res = run_lru(ref_str, capacity);
                        else if (algo == "OPTIMAL") res = run_optimal(ref_str, capacity);
                        else res = build_result("UNKNOWN", capacity, 0, 0, {});

                        http_res = make_http_response(result_to_json(res));
                    } else {
                        // Compare mode (Runs FIFO, LRU, and OPTIMAL)
                        std::string compare_json = "[" + 
                            result_to_json(run_fifo(ref_str, capacity)) + "," +
                            result_to_json(run_lru(ref_str, capacity)) + "," +
                            result_to_json(run_optimal(ref_str, capacity)) + "]";

                        http_res = make_http_response(compare_json);
                    }
                } catch (...) {
                    http_res = make_http_response("{\"error\":\"Invalid request format\"}", 400);
                }

                send(client_fd, http_res.c_str(), (int)http_res.length(), 0);
            }
        }
        closesocket(client_fd);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}