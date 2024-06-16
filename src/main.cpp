#include <bits/stdc++.h>
#include <core.h>
#include <log.h>

void show_packet(char* data, int length) {
  for (int i = 0; i < length; i++)
    printf("%02x ", static_cast<unsigned char>(data[i]));
  printf("\n");
}

int tap_processor(char* pkt, int len, void* application_data) {
  return 0; /* A return verdict which tell the datapath to send via lport 0 */
}

int packet_processor(void* packet, int recv_lport, CallBackWrapper* callbacks, void* application_data) {
  cout << "received a packet" << endl;
  struct ether_header* eth    = (struct ether_header*)callbacks->GetDataAtOffset(packet, 0);
  struct ether_addr* src_addr = (struct ether_addr*)&eth->ether_shost;
  struct ether_addr* dst_addr = (struct ether_addr*)&eth->ether_dhost;
  struct ether_addr tmp;

  tmp       = *src_addr;
  *src_addr = *dst_addr;
  *dst_addr = tmp;
  return recv_lport;
}


int main(int argc, char** argv) {
  DataPlaneManager dataplanemanager;

  dataplanemanager.load_and_parse_config_file(argv[1]);
  dataplanemanager.register_packet_handler(packet_processor);
  dataplanemanager.register_tap_handler(tap_processor);

  char tap_dev_name[] = "UPF_TUN0";
  char tap_ip_addr[]  = "192.168.1.1";
  dataplanemanager.create_multiqueue_tap_device(tap_dev_name, tap_ip_addr, 24);

  dataplanemanager.start_datapath_core();
  return 0;
}
