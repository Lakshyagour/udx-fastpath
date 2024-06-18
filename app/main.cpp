#include "include/gtp_header.h"
#include "include/upf_gtp.h"

#include <bits/stdc++.h>
#include <core.h>
#include <log.h>


unsigned int str_ip_addr_to_int(string ipadr) {
  unsigned int num = 0, val;
  char *tok, *ptr;
  tok = strtok((char*)ipadr.c_str(), ".");
  while (tok != NULL) {
    val = strtoul(tok, &ptr, 0);
    num = (num << 8) + val;
    tok = strtok(NULL, ".");
  }
  return (num);
}

ether_addr str_mac_addr_to_array(string macaddr) {
  struct ether_addr ether_addr;
  char* ptr;

  ether_addr.ether_addr_octet[0] = strtol(strtok((char*)macaddr.c_str(), ":"), &ptr, 16);
  for (uint8_t i = 1; i < 6; i++) {
    ether_addr.ether_addr_octet[i] = strtol(strtok(NULL, ":"), &ptr, 16);
  }
  return ether_addr;
}

int parse_application_config_file(const char* filename, ApplicationInfo& app_info) {
  ifstream ifs(filename);
  Reader reader;
  Value configuration;
  reader.parse(ifs, configuration);

  for (Json::Value::const_iterator it = configuration.begin(); it != configuration.end(); ++it) {
    if (it.key().asString() == APP_TAG) {
      app_info.ran_ip      = str_ip_addr_to_int(configuration[it.key().asString()][RAN_IP_TAG].asString());
      app_info.dnn_ip      = str_ip_addr_to_int(configuration[it.key().asString()][DNN_IP_TAG].asString());
      app_info.ran_mac     = str_mac_addr_to_array(configuration[it.key().asString()][RAN_MAC_TAG].asString());
      app_info.dnn_mac     = str_mac_addr_to_array(configuration[it.key().asString()][DNN_MAC_TAG].asString());
      app_info.upf_ran_mac = str_mac_addr_to_array(configuration[it.key().asString()][UPF_RAN_MAC_TAG].asString());
      app_info.upf_dnn_mac = str_mac_addr_to_array(configuration[it.key().asString()][UPF_DNN_MAC_TAG].asString());
    }
  }
  return 0;
}

int main(int argc, char** argv) {

  ApplicationInfo application_info;
  parse_application_config_file("udx-configs/application-config.jsonc", application_info);

  DataPlaneManager dataplanemanager;

  dataplanemanager.load_and_parse_config_file(argv[1]);
  dataplanemanager.register_packet_handler(packet_processor);
  dataplanemanager.register_tap_handler(tap_processor);
  dataplanemanager.register_application_data((void*)&application_info);

  char tap_dev_name[] = "UPF_TUN0";
  char tap_ip_addr[]  = "192.168.1.1";
  dataplanemanager.create_multiqueue_tap_device(tap_dev_name, tap_ip_addr, 24);

  dataplanemanager.start_datapath_core();
  return 0;
}
