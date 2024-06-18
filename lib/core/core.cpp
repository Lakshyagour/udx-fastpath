#include <common.h>
#include <core.h>
#include <fstream>
#include <jsoncpp/json/value.h>
#include <log.h>
#include <tap.h>
#include <unet.h>

/********** DataPlanerManager Functions **********/

bool DataPlaneManager::is_running() {
  if (QUIT == 0)
    return true;
  return false;
}

bool DataPlaneManager::is_started() {
  if (STARTED == 0)
    return true;
  return false;
}

int DataPlaneManager::load_and_parse_config_file(char* config_file_path) {
  log_info("Parsing Config File");
  Value configuration;
  ifstream ifs(config_file_path);
  Reader reader;

  reader.parse(ifs, configuration);

  if (!configuration.isMember(API_TAG)) {
    log_error("Required API TAG not found in configuration file\n");
    return ERROR_EXIT;
  }

  if (!configuration[API_TAG].isString()) {
    log_error("API_TAG type should be string");
    return ERROR_EXIT;
  }

  if (!configuration.isMember(MODEL_TAG)) {
    log_error("Required MODEL TAG not found in configuration file\n");
    return ERROR_EXIT;
  }

  if (!configuration[MODEL_TAG].isString()) {
    log_error("MODEL_TAG types should be a string");
    return ERROR_EXIT;
  }

  if (!configuration.isMember(LOG_LEVEL_TAG)) {
    log_warn("Required LOG_LEVEL TAG was not found in configuration file, "
             "defaulting to DEBUG");
    log_set_level(LOG_DEBUG);
  }

  if (configuration.isMember(CREATE_TAP_TAG)) {
    create_tap = configuration[CREATE_TAP_TAG].asBool();
  }
  if (configuration.isMember(TAP_DEVICE_NUM_QUEUES_TAG)) {
    num_queues_tap = configuration[TAP_DEVICE_NUM_QUEUES_TAG].asInt();
  }

  if (configuration[API_TAG].asString() == API_VALUE_CNDP_TAG) {
    if (!configuration.isMember(CNDP_DATAPATH_TAG)) {
      log_error("CNDP datapath selected but no configuration given");
      return ERROR_EXIT;
    }
    if (validate_cndp_configuration(configuration[CNDP_DATAPATH_TAG])) {
      log_error("CNDP Datapath Configuration Is Invalid");
      return ERROR_EXIT;
    }

    API                    = API_VALUE_CNDP;
    datapath_configuration = configuration[CNDP_DATAPATH_TAG];
    callbackwrapper        = new CndpCallBackWrapper();
  } else if (configuration[API_TAG].asString() == API_VALUE_DPDK_TAG) {
    if (!configuration.isMember(DPDK_DATAPATH_TAG)) {
      log_error("DPDK datapath selected but no configuration given");
      return ERROR_EXIT;
    }

    if (validate_dpdk_configuration(configuration[DPDK_DATAPATH_TAG])) {
      log_error("DPDK Datapath Configuration Is Invalid");
    }

    API                    = API_VALUE_DPDK;
    datapath_configuration = configuration[DPDK_DATAPATH_TAG];
    callbackwrapper        = new DPDKCallBackWrapper();
  }
  // else if (configuration[API_TAG].asString() == API_VALUE_AFPACKET_TAG) {
  //   if (!configuration.isMember(AFPACKET_DATAPATH_TAG)) {
  //     log_error("AFPACKET datapath selected but no configuration given");
  //     return ERROR_EXIT;
  //   }

  //   if
  //   (validate_afpacket_configuration(configuration[AFPACKET_DATAPATH_TAG])) {
  //     log_error("AFPACKET Datapath Configuration Is Invalid");
  //   }
  //   API = API_VALUE_AFPACKET;
  //   datapath_configuration = configuration[AFPACKET_DATAPATH_TAG];
  //   callbackwrapper = new AFPacketCallBackWrapper();
  // }
  else {
    log_info("Unknown Datapath --%s-- is selected", configuration[API_TAG].asCString());
    return ERROR_EXIT;
  }

  if (configuration[MODEL_TAG].asString() == MODEL_VALUE_MASTER_WORKER_TAG) {
    if (!configuration.isMember(WORKER_CONFIGURATION_TAG)) {
      log_error("Master Worker Model Selected But No Configuration Is Given");
      return ERROR_EXIT;
    }

    if (validate_workers_configuration(WORKER_CONFIGURATION_TAG)) {
      log_error("Worker Configuration Is Invalid");
      return ERROR_EXIT;
    }
    MODEL                = MODEL_VALUE_MASTER_WORKER;
    worker_configuration = configuration[WORKER_CONFIGURATION_TAG];
  } else if (configuration[MODEL_TAG].asString() == MODEL_VALUE_RUN_TO_COMPLETION_TAG) {
    MODEL = MODEL_VALUE_RUN_TO_COMPLETION;
  } else {
    log_error("Unknown Model --%s-- is selected");
    return ERROR_EXIT;
  }

  if (configuration.isMember(FASTPATH_PORT_CONFIGURATION_TAG)) {
    if (validate_fastpath_port_configuration(configuration[FASTPATH_PORT_CONFIGURATION_TAG])) {
      log_error("FastPathPort Configuration is Invalid");
      return ERROR_EXIT;
    }
    Value fastpath_port_configuration = configuration[FASTPATH_PORT_CONFIGURATION_TAG];
    if (!fastpath_port_configuration[FASTPATH_PORTS_TAG].isArray()) {
      log_error("FASTPATH_PORTS type is invalid must be an array type");
      return ERROR_EXIT;
    }

    fast_path_filtering = fastpath_port_configuration[FASTPATH_FILTERING_TAG].asBool();
    auto it             = fastpath_port_configuration[FASTPATH_PORTS_TAG].begin();
    while (it != fastpath_port_configuration[FASTPATH_PORTS_TAG].end()) {
      if ((*it).isInt64()) {
        fastpath_ports.insert((*it).asInt64());
      } else if ((*it).isString()) {
        string ports = (*it).asString();
        int ind      = ports.find_first_of('-');
        int start    = stoi(string(ports.begin(), ports.begin() + ind));
        int end      = stoi(string(ports.begin() + ind + 1, ports.end()));
        for (int i = start; i <= end; i++) {
          fastpath_ports.insert(i);
        }
      } else {
        log_warn("Unidentified type in FASTPATH_PORTS array, %s will not take effect must be INT "
                 "or String",
        (*it).asCString());
      }
      it++;
    }
  }
  return 0;
}

int DataPlaneManager::validate_cndp_configuration(Value configuration) { return 0; }

int DataPlaneManager::validate_dpdk_configuration(Value configuration) { return 0; }

int DataPlaneManager::validate_afpacket_configuration(Value configuration) { return 0; }

int DataPlaneManager::validate_workers_configuration(Value configuration) { return 0; }

int DataPlaneManager::validate_fastpath_port_configuration(Value configuration) { return 0; }

int DataPlaneManager::start_datapath_core() {
  // Start TAP Device if required.

  switch (API) {
    case API_VALUE_DPDK:
      log_info("Launching DPDK Datapath CORE");
      start_dpdk_datapath();
      break;
    case API_VALUE_CNDP:
      log_info("Launching CNDP Datapath CORE");
      start_cndp_datapath();
      break;
    case API_VALUE_AFPACKET:
      log_info("Launching AFPACKET Datapath CORE");
      start_afpacket_datapath();
      break;
    default: log_error("Unknown API found in Configuration File\n");
  }
  return 0;
}

int DataPlaneManager::reload_datapath_core() { return 0; }

int DataPlaneManager::stop_datapath_core() {
  QUIT = 0;
  return 0;
}

void DataPlaneManager::register_packet_handler(PacketProcessor func) { packet_processor = func; }

void DataPlaneManager::register_packet_handler_chain(vector<PacketProcessor> chain) {
  packet_processor_chain = chain;
}

void DataPlaneManager::register_tap_handler(TapProcessor func) { tap_processor = func; }

void DataPlaneManager::register_application_data(void* data) { application_data = data; }

Value DataPlaneManager::get_worker_configuration() { return worker_configuration; }

Value DataPlaneManager::get_datapath_configuration() { return datapath_configuration; }

int DataPlaneManager::get_tap_device_fd() { return tap_fd; }

int DataPlaneManager::get_model() { return MODEL; }

uint8_t* DataPlaneManager::get_tap_ether_addr() { return tap_mac_addr; }

int DataPlaneManager::get_packet_port_number(char* data, int len) {
  ether_header* eth_addr = (ether_header*)(data);
  data                   = (char*)(eth_addr + 1);

  if (eth_addr->ether_type != htons(ETH_P_IP))
    return 0;

  struct iphdr* iph = (struct iphdr*)data;
  data              = (char*)(iph + 1);

  if (iph->protocol != (IPPROTO_UDP))
    return 0;

  struct udphdr* udph = (struct udphdr*)(data);
  data                = (char*)(udph + 1);

  return htons(udph->dest);
}

int DataPlaneManager::get_ether_type(char* data, int len) {
  struct hdr_lens hdr_lens;
  struct ethhdr* ethhdr = (struct ethhdr*)data;
  uint32_t packet_type  = PTYPE_L2_ETHER;
  uint16_t protocol     = 0;
  uint32_t offset       = 0;
  protocol              = ethhdr->h_proto;
  offset                = sizeof(struct ethhdr);
  hdr_lens.l2_len       = offset;

  if (protocol == htobe16(PID_ETH_ARP)) {
    return packet_type = PTYPE_L2_ETHER_ARP;
  }

  if (protocol == htobe16(PID_ETH_IP)) {
    struct iphdr* iphdr = (struct iphdr*)(data + offset);
    packet_type         = ptype_l3_ip(iphdr->ihl);
    hdr_lens.l3_len     = ipv4_hdr_len(iphdr);
    offset += hdr_lens.l3_len;

    protocol    = iphdr->protocol;
    packet_type = ptype_l4(protocol);
  }

  if (packet_type == PTYPE_L4_UDP) {
    struct udphdr* udphdr = (struct udphdr*)(data + offset);
    hdr_lens.l4_len       = sizeof(struct udphdr);
    offset += hdr_lens.l4_len;
    packet_type = PTYPE_L4_UDP;

    if (udphdr->dest == be16toh(GTP_PORT)) {
      packet_type = PTYPE_5G_GTP;
    } else if (udphdr->dest == be16toh(PFCP_PORT)) {
      packet_type = PTYPE_5G_PFCP;
    }
  } else if (packet_type == PTYPE_L4_TCP) {
    struct tcphdr* tcphdr = (struct tcphdr*)(data + offset);
    hdr_lens.l4_len       = (tcphdr->doff & 0xf0) >> 2;
    offset += hdr_lens.l4_len;
    packet_type = PTYPE_L4_TCP;
  }
  return packet_type;
}

int DataPlaneManager::filter_packet(int port_number) { return fastpath_ports.count(port_number); }
bool DataPlaneManager::is_fastpath_filtering_enabled() { return fast_path_filtering; }

int DataPlaneManager::create_multiqueue_tap_device(char* iface_name, char* ipaddr, int prefix_bits) {
  if (!create_tap) {
    cout << "Create Tun Device set to false" << endl;
    return 0;
  }

  log_info("Creating Tun Device");

  if (create_virtual_device(iface_name, IFF_TAP | IFF_MULTI_QUEUE, (char*)tap_mac_addr, 1, tap_fd)) {
    log_error("Tap Device Creation Failed");
    exit(EXIT_FAILURE);
  }


  int netlink_fd = netlink_connect();
  if (netlink_fd == -1) {
    log_error("netlink_connect %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (netlink_set_addr_ipv4(netlink_fd, iface_name, ipaddr, prefix_bits) < 0) {
    log_error("netlink_set_addr_ipv4 %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (netlink_link_up(netlink_fd, iface_name) < 0) {
    log_error("netlink_link_up %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  print_mac_address(tap_mac_addr);
  ip_str_to_uint32(ipaddr, tap_ip_addr);

  close(netlink_fd);
  return 0;
}
void DataPlaneManager::print_configuration() {
  cout << worker_configuration.toStyledString();
  cout << datapath_configuration.toStyledString();
}


void print_arp_packet(struct cne_arp_hdr* arp_packet) {
  printf("ARP Packet:\n");
  printf("  |-Hardware type: %u\n", ntohs(arp_packet->arp_hardware));
  printf("  |-Protocol type: %u\n", ntohs(arp_packet->arp_protocol));
  printf("  |-Hardware size: %u\n", arp_packet->arp_hlen);
  printf("  |-Protocol size: %u\n", arp_packet->arp_plen);
  printf("  |-Opcode: %u\n", ntohs(arp_packet->arp_opcode));

  printf("  |-Sender MAC: ");
  print_mac_address(arp_packet->arp_data.arp_sha.ether_addr_octet);
  struct in_addr sender_ip;
  sender_ip.s_addr = arp_packet->arp_data.arp_sip;
  printf("  |-Sender IP: %s\n", inet_ntoa(sender_ip));
  struct in_addr target_ip;
  target_ip.s_addr = arp_packet->arp_data.arp_tip;
  printf("  |-Target MAC: ");
  print_mac_address(arp_packet->arp_data.arp_tha.ether_addr_octet);
  printf("  |-Target IP: %s\n", inet_ntoa(target_ip));
}

void construct_arp_response(struct cne_arp_hdr* arp, const uint8_t* src_mac, uint32_t src_ip, const uint8_t* dst_mac, uint32_t dst_ip) {
  arp->arp_hardware = htons(1);        // Ethernet
  arp->arp_protocol = htons(ETH_P_IP); // IPv4
  arp->arp_hlen     = 6;               // MAC address length
  arp->arp_plen     = 4;               // IPv4 address length
  arp->arp_opcode   = htons(2);        // ARP reply

  memcpy(arp->arp_data.arp_sha.ether_addr_octet, src_mac, 6);
  arp->arp_data.arp_sip = src_ip;
  memcpy(arp->arp_data.arp_tha.ether_addr_octet, dst_mac, 6);
  arp->arp_data.arp_tip = dst_ip;
}

int DataPlaneManager::arp_handler(char* buff, int recv_port) {

  struct cne_arp_hdr* arp_packet = (cne_arp_hdr*)(buff + ETH_HLEN);
  struct ethhdr* eth_hdr         = (struct ethhdr*)(buff);


  if (ntohs(arp_packet->arp_opcode) == 1) {
    uint32_t src_ip  = tap_ip_addr;
    uint32_t dest_ip = arp_packet->arp_data.arp_sip;

    unsigned char src_mac[ETH_ALEN];
    unsigned char dest_mac[ETH_ALEN];

    memcpy(src_mac, lport_mac[recv_port].ether_addr_octet, ETH_ALEN);
    memcpy(dest_mac, arp_packet->arp_data.arp_sha.ether_addr_octet, ETH_ALEN);

    if (arp_packet->arp_data.arp_tip == tap_ip_addr) {
      construct_arp_response(arp_packet, src_mac, src_ip, dest_mac, dest_ip);
      memcpy(eth_hdr->h_dest, eth_hdr->h_source, ETH_ALEN);
      memcpy(eth_hdr->h_source, src_mac, ETH_ALEN);
      return recv_port;
    }
    return DROP;
  } else if (ntohs(arp_packet->arp_opcode) == 2) {
    memcpy(eth_hdr->h_dest, tap_mac_addr, ETH_ALEN);
    memcpy(arp_packet->arp_data.arp_tha.ether_addr_octet, tap_mac_addr, ETH_ALEN);
    print_arp_packet(arp_packet);
    return KERNEL;
  }
  return DROP;
};
