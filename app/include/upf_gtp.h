
#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <core.h>
#include <iostream>
#include <map>
#include <net/ethernet.h>
#include <netinet/ether.h>

#define APP_TAG "application"
#define RAN_IP_TAG "ran-ip"
#define DNN_IP_TAG "dnn-ip"
#define RAN_MAC_TAG "ran-mac"
#define DNN_MAC_TAG "dnn-mac"
#define UPF_RAN_MAC_TAG "upf-ran-mac"
#define UPF_DNN_MAC_TAG "upf-dnn-mac"
#define ARP_REPLY_RAN_TAG "arp-reply-ran-interface"
#define ARP_REPLY_DNN_TAG "arp-reply-dnn-interface"
#define ARP_TABLE_TAG "arp-table"

class ApplicationInfo {
  public:
  uint32_t ran_ip;
  uint32_t dnn_ip;
  struct ether_addr ran_mac;
  struct ether_addr dnn_mac;
  struct ether_addr upf_ran_mac;
  struct ether_addr upf_dnn_mac;

  void print() {
    std::cout << "ran_ip: " << std::hex << ran_ip << std::dec << std::endl;
    std::cout << "dnn_ip: " << std::hex << dnn_ip << std::dec << std::endl;

    std::cout << "ran_mac: " << ether_ntoa(&ran_mac) << std::endl;
    std::cout << "dnn_mac: " << ether_ntoa(&dnn_mac) << std::endl;
    std::cout << "upf_ran_mac: " << ether_ntoa(&upf_ran_mac) << std::endl;
    std::cout << "upf_dnn_mac: " << ether_ntoa(&upf_dnn_mac) << std::endl;
  }
};

int parse_application_config_file(const char*, ApplicationInfo&);
int packet_processor(void* packet, int recv_lport, CallBackWrapper* callbacks, void* application_data);
int tap_processor(char* pkt, int len, void* application_data);
uint32_t parse_gtphdr(char* data, struct gtpheader** gtphdr_global, struct pdu_extension_header* pdu_extension_header);

#endif