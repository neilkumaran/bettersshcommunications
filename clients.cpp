// Runs in the background, sends and recieves info from the server. Run with app.cpp

#include <iostream>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>

using namespace std;

bool vpnCheck(const char* ifname) {
    // vpn filtering for tuns
    return (std::strncmp(ifname, "tun", 3) == 0 || std::strncmp(ifname, "wg", 2) == 0);
}

bool isVirtual(const char* ifname) {
    // virtual interfaces that need to be not added
    return (std::strncmp(ifname, "virbr", 5) == 0 ||  // libvirt
            std::strncmp(ifname, "docker", 6) == 0 || // docker bridge
            std::strncmp(ifname, "vboxnet", 7) == 0); // VirtualBox
}

bool isPhysical(const char* ifname) {
    // the good stuff
    return (std::strncmp(ifname, "eth", 3) == 0 ||
            std::strncmp(ifname, "enp", 3) == 0 ||
            std::strncmp(ifname, "wlan", 4) == 0 ||
            std::strncmp(ifname, "wl", 2) == 0);
}

std::string getLocalAddrInfo() {
    ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    std::string result;

    for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            sockaddr_in *sa = (sockaddr_in*)ifa->ifa_addr;
            char addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));

            if (std::strcmp(addr, "127.0.0.1") == 0) continue; // skip loopback
            if (vpnCheck(ifa->ifa_name)) continue;                // skip VPNs
            if (!isPhysical(ifa->ifa_name)) continue;         // only physical NICs
            if (isVirtual(ifa->ifa_name)) continue;           // skip virtual bridges

            result = addr;  // take the first matching LAN IP
            break;
        }
    }

    freeifaddrs(ifaddr);
    return result; // empty string if none found
}


int main() {
    cout << getLocalAddrInfo(); 
}
