#include <iostream>
#include <string>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

std::string getIPAddress(const std::string& interfaceName) {
    struct ifaddrs *ifaddr, *ifa;
    char addrBuffer[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET && interfaceName == ifa->ifa_name) {
            void *addrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, addrBuffer, INET_ADDRSTRLEN);
            freeifaddrs(ifaddr);
            return std::string(addrBuffer);
        }
    }

    freeifaddrs(ifaddr);
    return "";
}

int main() {
    std::string interfaceName = "tap0"; // Replace with your interface name
    std::string ipAddress = getIPAddress(interfaceName);

    if (!ipAddress.empty()) {
        std::cout << "IP address of " << interfaceName << ": " << ipAddress << std::endl;
    } else {
        std::cout << "Could not find IP address for interface: " << interfaceName << std::endl;
    }

    return 0;
}
