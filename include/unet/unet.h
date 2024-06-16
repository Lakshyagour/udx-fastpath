#ifndef __UNET_H__
#define __UNET_H__

#include <net/cne_arp.h>
#include <net/cne_ip.h>
#include <net/ethernet.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
struct packet_info {
  struct hdr_lens;
  struct ethhdr;
  struct iphdr;
  struct udphdr;
  struct tcphdr;
};

struct hdr_lens {
  int l2_len;
  int l3_len;
  int l4_len;
  int inner_l2_len;
  int inner_l3_len;
  int inner_l4_len;
};


/* Packet Types */
#define PTYPE_L2_ETHER 0x00000001
#define PTYPE_L2_ETHER_ARP 0x00000002
#define PTYPE_L2_ETHER_IPV4 0x00000004
#define PTYPE_L3_IPV4 0x00000005
#define PTYPE_L3_IPV4_EXT 0x00000006
#define PTYPE_L3_IPV6 0x00000007
#define PTYPE_L4_UDP 0x00000008
#define PTYPE_L4_TCP 0x00000009
#define PTYPE_5G_GTP 0x00000010
#define PTYPE_5G_PFCP 0x00000011

/* Protocol IDs */
#define PID_ETH_ARP ETH_P_ARP
#define PID_ETH_IP ETH_P_IP

/* Application Ports */
#define GTP_PORT 2152
#define PFCP_PORT 8805

static uint32_t ptype_l3_ip(uint8_t ipv_ihl) {
  static uint32_t ptype_l3_ip_proto_map[256] = { 0 };
  ptype_l3_ip_proto_map[0x45]                = PTYPE_L3_IPV4;
  ptype_l3_ip_proto_map[0x46]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x47]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x48]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x49]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4A]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4B]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4C]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4D]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4E]                = PTYPE_L3_IPV4_EXT;
  ptype_l3_ip_proto_map[0x4F]                = PTYPE_L3_IPV4_EXT;
  return ptype_l3_ip_proto_map[ipv_ihl];
}

static uint32_t ptype_l4(uint8_t proto) {
  static uint32_t ptype_l4_proto[256] = { 0 };
  ptype_l4_proto[IPPROTO_UDP]         = PTYPE_L4_UDP;
  ptype_l4_proto[IPPROTO_TCP]         = PTYPE_L4_TCP;
  return ptype_l4_proto[proto];
}

static inline uint8_t ipv4_hdr_len(const struct iphdr* ipv4_hdr) {
  return (uint8_t)((ipv4_hdr->ihl & 0x0f) * 4);
}


#endif