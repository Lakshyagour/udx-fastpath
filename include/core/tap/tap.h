#ifndef _TUN_H_
#define _TUN_H_


#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>


#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>

#include <bits/stdc++.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>


int create_virtual_device(char* name, int flags, char* ether_addr, int num_queues, int& tap_fd);
int tap_write(int tap_fd, char* buf, int bufLen);
int netlink_connect();
int netlink_set_addr_ipv4(int netlink_fd, const char* iface_name, const char* address, uint8_t network_prefix_bits);
int netlink_link_up(int netlink_fd, const char* iface_name);


#endif
