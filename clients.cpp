// Runs in the background, sends and recieves info from the server. Run with app.cpp

#include <iostream>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <cstdlib>

using namespace std;

string getCfgPath() {
    const char* home = getenv("HOME");
    if (!home) home = "."; // fallback to current directory

    string dir = string(home) + "/.bettersshcommunications";

    // create directory if it doesn't exist
    struct stat st;
    if (stat(dir.c_str(), &st) == -1) {
        mkdir(dir.c_str(), 0755);
    }

    return dir + "/clients.cfg";
}

// vpn filtering for tuns
bool vpnCheck(const char* ifname) {
    return (strncmp(ifname, "tun", 3) == 0 || strncmp(ifname, "wg", 2) == 0);
}

// virtual interfaces that need to be not added
bool isVirtual(const char* ifname) {
    return (strncmp(ifname, "virbr", 5) == 0 ||  // libvirt
            strncmp(ifname, "docker", 6) == 0 || // docker bridge
            strncmp(ifname, "vboxnet", 7) == 0); // VirtualBox
}

bool isPhysical(const char* ifname) {
    // the good stuff
    return (strncmp(ifname, "eth", 3) == 0 ||
            strncmp(ifname, "enp", 3) == 0 ||
            strncmp(ifname, "wlan", 4) == 0 ||
            strncmp(ifname, "wl", 2) == 0);
}

string getLocalAddrInfo() {
    ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    string result;

    for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        sockaddr_in *sa = (sockaddr_in*)ifa->ifa_addr;
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));

        if (!strcmp(addr, "127.0.0.1")) continue; // skip loopback
        if (vpnCheck(ifa->ifa_name)) continue;    // skip VPNs
        if (!isPhysical(ifa->ifa_name)) continue; // only physical NICs
        if (isVirtual(ifa->ifa_name)) continue;   // skip virtual bridges

        result = addr;  // take the first matching LAN IP
        break;
    }

    freeifaddrs(ifaddr);
    return result; // empty string if none found
}

int sendHeartbeat(string clientIP, string hostName, string serverIP) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0) {
        perror("socket init error");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    string message = hostName + ", " + clientIP; 

    send(sock, message.c_str(), strlen(message.c_str()), 0);

    close(sock);
    return 0;
}

string readIPFromCfg(const string& cfgFile) {
    ifstream infile(cfgFile);
    string ip;
    getline(infile, ip);
    return ip;
}

void writeIPToCfg(const string& cfgFile, const string& ip) {
    ofstream outfile(cfgFile);
    outfile << ip << endl;
}

int main() {
    string cfgFile = getCfgPath();

    string serverIP;
    string clientIP = getLocalAddrInfo();
    const char* username = getenv("USER");

    // Try to read IP from cfg, else prompt
    ifstream infile(cfgFile);
    if (infile.good()) {
        getline(infile, serverIP);
        infile.close();
    }
    if (serverIP.empty()) {
        cout << "Enter server IP: ";
        getline(cin, serverIP);
        writeIPToCfg(cfgFile, serverIP);
    }

    cout << "\nChange the IP at " << cfgFile << "\n";

    while (true) {
        sendHeartbeat(clientIP, username, serverIP);
        sleep(6.3);
    }
}  
