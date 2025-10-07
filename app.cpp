#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <termios.h>
#include <fcntl.h>
#include <filesystem>


void createGuestAccount() {
    std::cout << "Create Guest Account\n";

    // Add guest user
    int ret = system("sudo useradd -m guest");

    if (ret != 0) {
        std::cerr << "Failed to create guest account. "
                     "Maybe it already exists, or you need sudo.\n";
        return;
    }

    std::cout << "Guest account created successfully.\n";
    std::cout << "Now you need to set a password for the guest user.\n";
    std::cout << "Remember this password or you won’t be able to log in as guest.\n\n";

    // Run passwd to set password interactively
    ret = system("sudo passwd guest");

    if (ret == 0) {
        std::cout << "Password set for guest. Guest account is ready.\n";
    } else {
        std::cerr << "Failed to set password for guest.\n";
    }
}


void installClientService() {
    const char* home = getenv("HOME");
    if (!home) {
        std::cerr << "Error: HOME not set.\n";
        return;
    }

    std::string folder = std::string(home) + "/.bettersshcommunications";
    std::string clientBinary = folder + "/clients";

    // Ensure ~/.bettersshcommunications exists
    std::filesystem::create_directories(folder);

    // Compile clients.cpp into ~/.bettersshcommunications/clients
    std::string compileCmd = "g++ clients.cpp -o " + clientBinary;
    int compileRet = system(compileCmd.c_str());
    if (compileRet != 0) {
        std::cerr << "Compilation failed. Check clients.cpp\n";
        return;
    }

    std::cout << "Compiled client to " << clientBinary << "\n";

    // Create ~/.config/systemd/user if needed
    std::string serviceDir = std::string(home) + "/.config/systemd/user";
    std::filesystem::create_directories(serviceDir);

    std::string servicePath = serviceDir + "/clients.service";

    // Write the service file
    std::ofstream serviceFile(servicePath);
    if (!serviceFile.is_open()) {
        std::cerr << "Failed to create service file at " << servicePath << "\n";
        return;
    }

    serviceFile << "[Unit]\n";
    serviceFile << "Description=BetterSSH Client Service\n";
    serviceFile << "After=network.target\n\n";
    serviceFile << "[Service]\n";
    serviceFile << "ExecStart=" << clientBinary << "\n";
    serviceFile << "Restart=always\n";
    serviceFile << "RestartSec=5\n\n";
    serviceFile << "[Install]\n";
    serviceFile << "WantedBy=default.target\n";
    serviceFile.close();

    // Enable and start service
    std::string cmd = "systemctl --user daemon-reload && "
                      "systemctl --user enable --now clients.service";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to enable/start service.\n";
        std::cerr << "Try manually running:\n";
        std::cerr << "systemctl --user daemon-reload && systemctl --user enable --now clients.service\n";
    } else {
        std::cout << "Client service installed and started!\n";
    }
}

void removeClientService() {
    const char* home = getenv("HOME");
    if (!home) {
        std::cerr << "Error: HOME not set.\n";
        return;
    }

    std::string servicePath = std::string(home) + "/.config/systemd/user/clients.service";
    std::string clientBinary = std::string(home) + "/.bettersshcommunications/clients";

    // Stop and disable service
    std::string cmd = "systemctl --user stop clients.service && "
                      "systemctl --user disable clients.service";
    system(cmd.c_str());

    // Remove service file
    if (std::filesystem::exists(servicePath)) {
        std::filesystem::remove(servicePath);
        std::cout << "Removed service file: " << servicePath << "\n";
    }

    // Remove binary
    if (std::filesystem::exists(clientBinary)) {
        std::filesystem::remove(clientBinary);
        std::cout << "Removed binary: " << clientBinary << "\n";
    }

    std::cout << "Client service removed.\n";
}

void checkClientService() {
    // Runs "systemctl --user is-active clients.service"
    int ret = system("systemctl --user is-active --quiet clients.service");
    if (ret == 0) {
        std::cout << "Client service is running.\n";
    } else {
        std::cout << "Client service is not running.\n";
    }
}

void clearScreen() {
    std::cout << "\033[2J\033[H"; // clear screen and reset cursor
}

std::string getServerIP() {
    const char* home = getenv("HOME");
    if (!home) return "";

    std::string path = std::string(home) + "/.bettersshcommunications/clients.cfg";
    std::ifstream infile(path);
    if (!infile.is_open()) return "";

    std::string ip;
    getline(infile, ip);
    infile.close();
    return ip;
}

std::string requestClients(const std::string& serverIP) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "Socket creation failed.\n";

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return "Failed to connect to server.\n";
    }

    std::string request = "GET_CLIENTS";
    send(sock, request.c_str(), request.size(), 0);

    char buffer[8192];
    ssize_t n = read(sock, buffer, sizeof(buffer)-1);
    std::string response;
    if (n > 0) {
        buffer[n] = '\0';
        response = buffer;
    } else {
        response = "No data received.\n";
    }

    close(sock);
    return response;
}

// Check if a key has been pressed (non-blocking)
int kbhit() {
    termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}


void viewUsers() {
    std::string serverIP = getServerIP();
    if (serverIP.empty()) {
        std::cout << "Server IP not found in ~/.bettersshcommunications/clients.cfg\n";
        return;
    }

    std::cout << "Launching live monitor (press 'q' to go back)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    while (true) {
        if (kbhit()) {
            char c = getchar();
            if (c == 'q' || c == 'Q') break; // exit live monitor
        }

        clearScreen();
        std::string clientsData = requestClients(serverIP);
        std::cout << "Connected Clients\n";
        std::cout << clientsData << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void showMenu() {
    while (true) {
        clearScreen();
        std::cout << "BetterSSH Communications\n";
        std::cout << "1. View Users\n";
        std::cout << "2. Change Server IP (coming soon)\n";
        std::cout << "3. Install Client\n";
        std::cout << "4. Remove Client\n";
        std::cout << "5. Check Client Status\n";
        std::cout << "6. Create Guest Account (Required for safe SSHing)\n";
        std::cout << "7. Exit\n";
        std::cout << "Enter your choice: ";

        int choice;
        std::cin >> choice;

        switch (choice) {
            case 1:
                viewUsers();
                break;
            case 2:
                std::cout << "Change Server IP functionality coming soon.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            case 3:
                installClientService();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            case 4:
                removeClientService();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            case 5:
                checkClientService();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            case 6:
                createGuestAccount();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break; 
            case 7:
                return;
            default:
                std::cout << "Invalid choice.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}


int main() {
    showMenu();
    return 0;
}
