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

void swap_mac_addresses(void* data) {
  struct ether_header* eth    = (struct ether_header*)data;
  struct ether_addr* src_addr = (struct ether_addr*)&eth->ether_shost;
  struct ether_addr* dst_addr = (struct ether_addr*)&eth->ether_dhost;
  struct ether_addr tmp;

  tmp       = *src_addr;
  *src_addr = *dst_addr;
  *dst_addr = tmp;
}

uint32_t get_packet_type(char *data, struct hdr_lens *hdr_lens)
{
    struct ethhdr *ethhdr = (struct ethhdr *)data;
    uint32_t packet_type = PTYPE_L2_ETHER;
    uint16_t protocol = 0;
    uint32_t offset = 0;
    protocol = ethhdr->h_proto;
    offset = sizeof(struct ethhdr);
    hdr_lens->l2_len = offset;

    if (protocol == htobe16(PID_ETH_ARP))
    {
        return packet_type = PTYPE_L2_ETHER_ARP;
    }

    if (protocol == htobe16(PID_ETH_IP))
    {
        struct iphdr *iphdr = (struct iphdr *)(data + offset);
        packet_type = ptype_l3_ip(iphdr->ihl);
        hdr_lens->l3_len = ipv4_hdr_len(iphdr);
        offset += hdr_lens->l3_len;

        protocol = iphdr->protocol;
        packet_type = ptype_l4(protocol);
    }

    if (packet_type == PTYPE_L4_UDP)
    {
        struct udphdr *udphdr = (struct udphdr *)(data + offset);
        hdr_lens->l4_len = sizeof(struct udphdr);
        offset += hdr_lens->l4_len;
        packet_type = PTYPE_L4_UDP;

        if (udphdr->dest == be16toh(GTP_PORT))
        {
            packet_type = PTYPE_5G_GTP;
        }
        else if (udphdr->dest == be16toh(PFCP_PORT))
        {
            packet_type = PTYPE_5G_PFCP;
        }
    }
    else if (packet_type == PTYPE_L4_TCP)
    {
        struct tcphdr *tcphdr = (struct tcphdr *)(data + offset);
        hdr_lens->l4_len = (tcphdr->doff & 0xf0) >> 2;
        offset += hdr_lens->l4_len;
        packet_type = PTYPE_L4_TCP;
    }
    return packet_type;
}


