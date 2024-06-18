
#include "include/upf_gtp.h"
#include "include/gtp_header.h"
#include <core.h>
#include <log.h>
#include <shm.h>
#include <unet.h>

#define LINE_BREAK printf("#############################################################################\n")

using namespace Json;
using namespace std;
using namespace boost;

typedef pair<const struct pdi_info_uplink, struct pdr_value> value_type_uplink;
typedef interprocess::allocator<value_type_uplink, interprocess::managed_shared_memory::segment_manager> shmem_allocator_uplink;
typedef interprocess::map<struct pdi_info_uplink, struct pdr_value, std::less<pdi_info_uplink>, shmem_allocator_uplink> uplink_map;

typedef pair<const struct pdi_info_downlink, struct pdr_value> value_type_downlink;
typedef interprocess::allocator<value_type_downlink, interprocess::managed_shared_memory::segment_manager> shmem_allocator_downlink;
typedef interprocess::map<struct pdi_info_downlink, struct pdr_value, std::less<pdi_info_downlink>, shmem_allocator_downlink> downlink_map;

interprocess::managed_shared_memory segment(interprocess::open_or_create, "SharedMemoryKey", 65536);
interprocess::offset_ptr<downlink_map> downlink_pdi_map = segment.find<downlink_map>("SharedDownlinkMap").first;
interprocess::offset_ptr<uplink_map> uplink_pdi_map     = segment.find<uplink_map>("SharedUplinkMap").first;

int tap_processor(char* pkt, int len, void* application_data) {
  log_debug("Received From Kernel via the tap sending to port 0");
  return 0; /* A return verdict which tell the datapath to send via lport 0 */
}

int packet_processor(void* packet, int recv_lport, CallBackWrapper* callbacks, void* application_data) {
  /** Core fast path logic comes here */
  ApplicationInfo* application_info = static_cast<ApplicationInfo*>(application_data);

  if (application_info == NULL) {
    log_error("Application data shold not be NULL\n");
    return DROP;
  }
  char* data = callbacks->GetDataAtOffset(packet, 0);
  struct hdr_lens hdr_lens;
  uint32_t packet_type = get_packet_type(data, &hdr_lens);
  int verdict          = DROP;

  switch (packet_type) {
    /* Process The Downlink Packet*/
    case PTYPE_L4_UDP: {
      verdict                          = recv_lport ^ 1;
      struct iphdr* iphdr              = (struct iphdr*)callbacks->GetDataAtOffset(packet, hdr_lens.l2_len);
      uint32_t dest_ip                 = (uint32_t)htonl(iphdr->daddr);
      struct pdi_info_downlink map_key = {0};

      map_key.srcIface     = 1;
      map_key.ueDestIpAddr = dest_ip;

      downlink_map ::iterator iter;
      iter = downlink_pdi_map->find(map_key);
      if (iter == downlink_pdi_map->end()) {
        printf("Failure in map lookup\n");
        return DROP;
      }

      struct pdr_value map_value = iter->second;
      uint8_t apply_action       = map_value.applyAction & (PFCP_APPLY_ACTION_FORW_MASK | PFCP_APPLY_ACTION_BUFF_MASK | PFCP_APPLY_ACTION_DROP_MASK);

      switch (apply_action) {
        case PFCP_APPLY_ACTION_FORW_MASK: {
          if (map_value.farMask & UPF_FAR_MASK_OHC) {

            size_t gtpoffset = sizeof(struct gtpheader);
            size_t pduoffset = sizeof(struct pdu_dl_extension_header);

            struct pdu_dl_extension_header* pdu_extension_hdr = (struct pdu_dl_extension_header*)callbacks->PrependData(packet, pduoffset);

            pdu_extension_hdr->len            = 1;
            pdu_extension_hdr->pduType        = 0;
            pdu_extension_hdr->qfi            = 1;
            pdu_extension_hdr->rqi            = 0;
            pdu_extension_hdr->nxtExtnHdrType = E_GTP_EXT_HDR_NO_MORE_EXTENSION_HEADERS;

            struct gtpheader* gtphdr = (struct gtpheader*)callbacks->PrependData(packet, gtpoffset);

            gtphdr->flags          = GTP_V1_MASK | PT_MASK | GTP_E_MASK;
            gtphdr->msgType        = G_PDU;
            gtphdr->length         = callbacks->GetDataLength(packet);
            gtphdr->teid           = map_value.ohc.teid;
            gtphdr->nxtExtnHdrType = E_GTP_EXT_HDR_PDU_SESSION_CONTAINER;

            struct udphdr* udphdr = (struct udphdr*)callbacks->PrependData(packet, UDP_HDR_LENGTH);
            udphdr->source        = 2152;
            udphdr->dest          = 2152;
            udphdr->len           = callbacks->GetDataLength(packet);
            udphdr->check         = 0;

            struct iphdr* iphdr = (struct iphdr*)callbacks->PrependData(packet, DEFAULT_IPV4_HDR_LENGTH);
            iphdr->version      = 4;
            iphdr->ihl          = 5;
            iphdr->protocol     = IPPROTO_UDP;
            iphdr->ttl          = 64;
            iphdr->saddr        = htonl(0xc0a80202);
            iphdr->daddr        = htonl(0xc0a80201);
            iphdr->tot_len      = htons(callbacks->GetDataLength(packet));
            iphdr->check        = 0;
            iphdr->tos          = 0;

            struct ethhdr* ethhdr = (struct ethhdr*)callbacks->PrependData(packet, ETH_HLEN);

            ethhdr->h_source[0] = application_info->upf_ran_mac.ether_addr_octet[0];
            ethhdr->h_source[1] = application_info->upf_ran_mac.ether_addr_octet[1];
            ethhdr->h_source[2] = application_info->upf_ran_mac.ether_addr_octet[2];
            ethhdr->h_source[3] = application_info->upf_ran_mac.ether_addr_octet[3];
            ethhdr->h_source[4] = application_info->upf_ran_mac.ether_addr_octet[4];
            ethhdr->h_source[5] = application_info->upf_ran_mac.ether_addr_octet[5];

            ethhdr->h_dest[0] = application_info->ran_mac.ether_addr_octet[0];
            ethhdr->h_dest[1] = application_info->ran_mac.ether_addr_octet[1];
            ethhdr->h_dest[2] = application_info->ran_mac.ether_addr_octet[2];
            ethhdr->h_dest[3] = application_info->ran_mac.ether_addr_octet[3];
            ethhdr->h_dest[4] = application_info->ran_mac.ether_addr_octet[4];
            ethhdr->h_dest[5] = application_info->ran_mac.ether_addr_octet[5];
            ethhdr->h_proto   = htobe16(PID_ETH_IP);
          } else if (map_value.farMask & UPF_PDR_MASK_OHR) {
          }
          break;
        }
        case PFCP_APPLY_ACTION_BUFF_MASK:
          /* TODO: In Future*/
          break;

        case PFCP_APPLY_ACTION_DROP_MASK:
          /* TODO: In Future*/
          break;
      }

      break;
    }
    /* Process The Uplink Packet*/
    case PTYPE_5G_GTP: {
      verdict                                       = recv_lport ^ 1;
      struct gtpheader* gtphdr                      = {0};
      struct pdu_extension_header pdu_extension_hdr = {0};

      char* gtp_start    = (char*)callbacks->GetDataAtOffset(packet, hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len);
      int gtp_header_len = parse_gtphdr(gtp_start, &gtphdr, &pdu_extension_hdr);
      if (gtphdr == NULL) {

        struct iphdr* iphdr = (struct iphdr*)callbacks->GetDataAtOffset(packet, hdr_lens.l2_len);
        uint32_t dest_ip    = (uint32_t)htonl(iphdr->daddr);

        log_error("GTP Header parsing failed what!!!!!!\n");
        return DROP;
      }
      char* payload_start = (char*)callbacks->GetDataAtOffset(packet, hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len + gtp_header_len);

      struct iphdr* inner_ipv4_hdr = (struct iphdr*)payload_start;
      uint32_t inner_src_ip        = (uint32_t)htonl(inner_ipv4_hdr->saddr);

      struct pdi_info_uplink map_key = {0};
      map_key.teid                   = (__u32)htonl(gtphdr->teid);
      map_key.ueIpAddr               = inner_src_ip;
      map_key.srcIface               = 0;

      if (pdu_extension_hdr.uplink_extension_header)
        map_key.qfi = pdu_extension_hdr.uplink_extension_header->qfi;
      else
        map_key.qfi = 0;

      uplink_map ::iterator iter;

      iter = uplink_pdi_map->find(map_key);
      if (iter == uplink_pdi_map->end()) {
        printf("MAP LOOKUP FAILED!!!!\n");
        return DROP;
      }
      struct pdr_value map_value = iter->second;
      __u8 apply_action          = map_value.applyAction & (PFCP_APPLY_ACTION_FORW_MASK | PFCP_APPLY_ACTION_BUFF_MASK | PFCP_APPLY_ACTION_DROP_MASK);

      switch (apply_action) {
        case PFCP_APPLY_ACTION_FORW_MASK:
          if (map_value.pdrMask & UPF_PDR_MASK_OHR) {
            struct ethhdr* ethhdr = (struct ethhdr*)callbacks->AdjustOffset(packet, hdr_lens.l3_len + hdr_lens.l4_len + gtp_header_len);

            if (ethhdr == NULL) {
              log_error("packet size %d", callbacks->GetDataLength(packet));
              log_error("Why thsi is null!!\n");
              return DROP;
            }
            ethhdr->h_source[0] = application_info->upf_dnn_mac.ether_addr_octet[0];
            ethhdr->h_source[1] = application_info->upf_dnn_mac.ether_addr_octet[1];
            ethhdr->h_source[2] = application_info->upf_dnn_mac.ether_addr_octet[2];
            ethhdr->h_source[3] = application_info->upf_dnn_mac.ether_addr_octet[3];
            ethhdr->h_source[4] = application_info->upf_dnn_mac.ether_addr_octet[4];
            ethhdr->h_source[5] = application_info->upf_dnn_mac.ether_addr_octet[5];

            ethhdr->h_dest[0] = application_info->dnn_mac.ether_addr_octet[0];
            ethhdr->h_dest[1] = application_info->dnn_mac.ether_addr_octet[1];
            ethhdr->h_dest[2] = application_info->dnn_mac.ether_addr_octet[2];
            ethhdr->h_dest[3] = application_info->dnn_mac.ether_addr_octet[3];
            ethhdr->h_dest[4] = application_info->dnn_mac.ether_addr_octet[4];
            ethhdr->h_dest[5] = application_info->dnn_mac.ether_addr_octet[5];
            ethhdr->h_proto   = htobe16(PID_ETH_IP);
          } else if (map_value.farMask & UPF_FAR_MASK_OHC) {
            /* TODO: In future */
          }
          break;

        case PFCP_APPLY_ACTION_BUFF_MASK:
          /* TODO: In future */
          break;

        case PFCP_APPLY_ACTION_DROP_MASK:
          /* TODO: In future */
          break;

        default:
          /* Invalid Case?? */
          break;
      }
      break;
    }
    case PTYPE_5G_PFCP: {
      /* Process the PFCP packet and send to the Control Plane */
      /* Send to UPF Control Plane a TUN device */
      verdict = KERNEL;
    }
    default:
      /* The packet is neither Uplink or Downlink packet.*/
      verdict = DROP;
      break;
  }
  return verdict;
}

uint32_t parse_gtphdr(char* data, struct gtpheader** gtphdr_global, struct pdu_extension_header* pdu_extension_header) {
  int len_gtp_header       = 0;
  struct gtpheader* gtphdr = (struct gtpheader*)data;
  len_gtp_header           = sizeof(struct gtpheader);


  int num_of_header_parsed   = 0;
  __u8 extension_header_type = gtphdr->nxtExtnHdrType;

  data += 12;

  while (extension_header_type != E_GTP_EXT_HDR_NO_MORE_EXTENSION_HEADERS && num_of_header_parsed < 1) {
    num_of_header_parsed++;
    switch (extension_header_type) {
      case E_GTP_EXT_HDR_PDU_SESSION_CONTAINER:
        // Get the PDU TYPE fronm nh->pos
        {
          __u8 pduType = (*(__u8*)(data + 1)) >> 4;

          switch (pduType) {
            case 0: // Downlink Packet
              // this means the nh->pos is pointing to the downlink pdu session container
              pdu_extension_header->downlink_extension_header =
              (pdu_dl_extension_header*)data;
              pdu_extension_header->pdu_type = 0;
              data += 8;
              len_gtp_header += 8;
              extension_header_type = pdu_extension_header->downlink_extension_header->nxtExtnHdrType;

              break;

            case 1: // Uplink Packet
              // this means nh->pos is pointing to the uplink pdu session container

              pdu_extension_header->uplink_extension_header =
              (pdu_ul_extension_header*)data;
              data += 4;
              len_gtp_header += 4;
              extension_header_type =
              pdu_extension_header->uplink_extension_header->nxtExtnHdrType;
              pdu_extension_header->pdu_type = 1;

              break;

            default:
              return -1;
          }
        }
        break;
      default:
        return -1;
    }
  }
  *gtphdr_global = gtphdr;
  return len_gtp_header;
}
