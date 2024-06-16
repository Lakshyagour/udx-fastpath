#ifndef __COMMON_H__
#define __COMMON_H__

#include <tap.h>
#include <unet.h>

void ip_str_to_uint32(char* ip_str, uint32_t& ip_int32);
void get_mac_address(const char* interface, uint8_t* mac);
void print_mac_address(uint8_t* addr);
void print_packet(char* data, int length);
int get_mac_address_sysfs(const char* interface, uint8_t* mac);

static inline void swap_mac_addresses(void* data) {
  struct ether_header* eth    = (struct ether_header*)data;
  struct ether_addr* src_addr = (struct ether_addr*)&eth->ether_shost;
  struct ether_addr* dst_addr = (struct ether_addr*)&eth->ether_dhost;
  struct ether_addr tmp;

  tmp       = *src_addr;
  *src_addr = *dst_addr;
  *dst_addr = tmp;
}
#endif