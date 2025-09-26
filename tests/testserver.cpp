#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fstream>
#include <map>
#include <mutex>
#include <chrono>

struct ClientInfo {
    std::string ip;
    std::chrono::steady_clock::time_point last_seen;
    bool online;
};

std::map<std::string, ClientInfo> clients;  // hostname → info
std::mutex clients_mutex;

void updateFile() {
    std::ofstream log("connections.txt");
    if (log.is_open()) {
        for (auto& [host, info] : clients) {
            log << host << ", " << info.ip << " - " 
                << (info.online ? "Online" : "Offline") << "\n";
        }
    }
}

void handleClient(int client_fd) {
    char buffer[1024] = {};
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        std::string msg(buffer);

        // Expect "hostname, ip"
        size_t comma = msg.find(',');
        if (comma != std::string::npos) {
            std::string host = msg.substr(0, comma);
            std::string ip = msg.substr(comma + 1);
            if (!ip.empty() && ip[0] == ' ') ip.erase(0, 1); // trim space

            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[host] = {ip, std::chrono::steady_clock::now(), true};
            updateFile();
        }
    }
    close(client_fd);
}

void watchdog() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto now = std::chrono::steady_clock::now();
            for (auto& [host, info] : clients) {
                if (info.online &&
                    std::chrono::duration_cast<std::chrono::seconds>(now - info.last_seen).count() > 15) {
                    info.online = false; // mark offline
                }
            }

            // Clear console and print table
            std::cout << "\033[2J\033[H"; // clear + reset cursor
            std::cout << "=== Current Clients ===\n";
            for (auto& [host, info] : clients) {
                std::cout << host << ", " << info.ip 
                          << " - " << (info.online ? "Online" : "Offline") << "\n";
            }
            std::cout << "=======================\n";

            updateFile();
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(12345);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

    std::cout << "Server listening on port 12345...\n";

    std::thread(watchdog).detach();

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) { perror("accept"); continue; }

        std::thread(handleClient, client_fd).detach();
    }

    close(server_fd);
}
