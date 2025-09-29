//This will act as the main TUI for the app that will control everything else.

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>  

// IP from ~/.bettersshcommunications/clients.cfg
std::string getServerIP() {
    const char* home = getenv("HOME");
    if (!home) return "";

    std::string path = std::string(home) + "/.bettersshcommunications/clients.cfg";
    std::ifstream infile(path);
    if (!infile.is_open()) {
        std::cerr << "Could not open " << path << "\n";
        return "";
    }

    std::string ip;
    getline(infile, ip);
    infile.close();
    return ip;
}

int main() {
    std::string serverIP = getServerIP();
    if (serverIP.empty()) {
        std::cerr << "Server IP not found.\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }


    std::string request = "GET_CLIENTS";
    send(sock, request.c_str(), request.size(), 0);

    // receive the response
    char buffer[8192];
    ssize_t n = read(sock, buffer, sizeof(buffer)-1);
    if (n > 0) {
        buffer[n] = '\0';
        std::cout << "=== Current Clients ===\n" << buffer << std::endl;
    } else {
        std::cout << "No data received.\n";
    }

    close(sock);
    return 0;
}
