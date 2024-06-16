#include <bits/stdc++.h>
#include <config_tags.h>
#include <jsoncpp/json/json.h>
#include <pktmbuf.h>
#include <rte_mbuf.h>
#include <unet.h>

using namespace std;
using namespace Json;

#ifndef __CORE_H__
#define __CORE_H__

#define KERNEL 1000  // Send the packet to kernel using tap device
#define DEFAULT 2000 // Send the packet to the same interface from which it came
#define DROP 3000    // Drop the packet and free the packet buffer
#define NEXT 4000    // Send the packet to next packet processor in the chain.
class CallBackWrapper {
  public:
  virtual uint16_t GetHeadRoom(void*)            = 0;
  virtual uint16_t GetTailRoom(void*)            = 0;
  virtual uint16_t GetDataLength(void*)          = 0;
  virtual char* GetDataAtOffset(void*, uint16_t) = 0;
  virtual char* PrependData(void*, uint16_t)     = 0;
  virtual char* AppendData(void*, uint16_t)      = 0;
  virtual char* AdjustOffset(void*, uint16_t)    = 0;
  virtual void* TrimPacket(void*, uint16_t)      = 0;
};

class DPDKCallBackWrapper : public CallBackWrapper {
  public:
  uint16_t GetHeadRoom(void*);
  uint16_t GetTailRoom(void*);
  uint16_t GetDataLength(void*);
  char* GetDataAtOffset(void*, uint16_t);
  char* PrependData(void*, uint16_t);
  char* AppendData(void*, uint16_t);
  char* AdjustOffset(void*, uint16_t);
  void* TrimPacket(void*, uint16_t);
};

class CndpCallBackWrapper : public CallBackWrapper {
  public:
  uint16_t GetHeadRoom(void*);
  uint16_t GetTailRoom(void*);
  uint16_t GetDataLength(void*);
  char* GetDataAtOffset(void*, uint16_t);
  char* PrependData(void*, uint16_t);
  char* AppendData(void*, uint16_t);
  char* AdjustOffset(void*, uint16_t);
  void* TrimPacket(void*, uint16_t);
};

// class AFPacketCallBackWrapper : public CallBackWrapper
// {
//  public:
//   uint16_t GetHeadRoom(void *);
//   uint16_t GetTailRoom(void *);
//   uint16_t GetDataLength(void *);
//   char *GetDataAtOffset(void *, uint16_t);
//   char *PrependData(void *, uint16_t);
//   char *AppendData(void *, uint16_t);
//   char *AdjustOffset(void *, uint16_t);
//   void *TrimPacket(void *, uint16_t);
// };

typedef int (*PacketProcessor)(void*, int, CallBackWrapper*, void*);
typedef int (*TapProcessor)(char*, int, void*);

class DataPlaneManager {
  int STARTED = 0;
  int QUIT    = 0;
  int API     = 0;
  int MODEL   = MODEL_VALUE_RUN_TO_COMPLETION;
  Value worker_configuration;
  Value datapath_configuration;
  Value fastpath_port_configuration;
  vector<ether_addr> lports_mac;

  int tap_fd         = -1;
  int create_tap     = false;
  int num_queues_tap = 1;
  uint32_t tap_ip_addr;
  uint8_t tap_mac_addr[ETH_ALEN];


  int validate_cndp_configuration(Value configuration);
  int validate_dpdk_configuration(Value configuration);
  int validate_afpacket_configuration(Value configuration);
  int validate_workers_configuration(Value configuration);
  int validate_fastpath_port_configuration(Value configuration);


  int start_dpdk_datapath();
  int start_cndp_datapath();

  int start_afpacket_datapath() { return 0; };

  public:
  vector<PacketProcessor> packet_processor_chain;
  PacketProcessor packet_processor;
  TapProcessor tap_processor;
  CallBackWrapper* callbackwrapper;
  void* application_data;

  DataPlaneManager() {}
  DataPlaneManager(DataPlaneManager&) {}

  int load_and_parse_config_file(char*);

  bool is_running();
  bool is_started();
  int start_datapath_core();
  int reload_datapath_core();
  int stop_datapath_core();
  int get_ether_type(char* buff, int len);
  int get_packet_port_number(char* buff, int len);
  int filter_packet(int port_number);
  int arp_handler(char*, int);

  int control_plane_launch(char* executable_path) { return 0; };

  void register_packet_handler(PacketProcessor func);
  void register_packet_handler_chain(vector<PacketProcessor> chain);
  void register_tap_handler(TapProcessor func);
  void register_application_data(void* f);

  int create_multiqueue_tap_device(char* iface_name, char* ipaddr, int prefix_bits);

  Value get_worker_configuration();
  Value get_datapath_configuration();
  uint8_t* get_tap_ether_addr();
  int get_tap_device_fd();
  int get_model();

  void print_configuration();
};

#endif
