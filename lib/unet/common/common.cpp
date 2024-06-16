#include <common.h>
#include <log.h>

using namespace std;
void ip_str_to_uint32(char* ip_str, uint32_t& ip_int32) {
  struct in_addr ip_addr;
  if (inet_pton(AF_INET, ip_str, &ip_addr) != 1) {
    std::cerr << "Invalid IP address format" << std::endl;
    exit(EXIT_FAILURE);
  }
  ip_int32 = ip_addr.s_addr;
}

void get_mac_address(const char* interface, uint8_t* mac) {
  int sockfd;
  struct ifreq ifr;

  // Create a socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    log_error("Error creating socket");
    return;
  }

  // Set interface name
  strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
  ifr.ifr_name[IFNAMSIZ - 1] = 0;

  // Retrieve the MAC address
  if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
    log_error("Error getting MAC address", strerror(errno));
    close(sockfd);
    return;
  }
  // Copy the MAC address to the provided array
  memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

  // Close the socket
  close(sockfd);
}

void print_mac_address(uint8_t* addr) {
  for (int i = 0; i < 6; ++i) {
    printf("%02x", addr[i]);
    if (i < 5)
      printf(":");
  }
  printf("\n");
}

void print_packet(char* data, int length) {
  for (int i = 0; i < length; i++)
    printf("%02x ", static_cast<unsigned char>(data[i]));
  printf("\n");
}

int get_mac_address_sysfs(const char* interface, uint8_t* mac) {
  ifstream inputFile("/sys/class/net/UPF_TUN0/address");
  if (!inputFile.is_open()) {
    cerr << "Error opening the file!" << endl;
    return 1;
  }

  string address;
  cout << "File Content: " << endl;
  while (getline(inputFile, address)) {
    cout << address << endl; // Print the current line
  }
  inputFile.close();
  return 0;

}
