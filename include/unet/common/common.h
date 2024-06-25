#ifndef __COMMON_H__
#define __COMMON_H__

#include <tap.h>
#include <unet.h>

using namespace std;
void ip_str_to_uint32(char* ip_str, uint32_t& ip_int32);
void get_mac_address(const char* interface, uint8_t* mac);
void print_mac_address(uint8_t* addr);
void print_packet(char* data, int length);
int get_mac_address_sysfs(const char* interface, uint8_t* mac);
string get_ip_address(char* interfaceName);
void swap_mac_addresses(void* data);
#endif