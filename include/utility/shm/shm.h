#include <arpa/inet.h>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <functional>
#include <iostream>
#include <utility>

#define PFCP_IPV4_ADDR_LEN 4
#define PFCP_IPV6_ADDR_LEN 16
#define UDP_HDR_LENGTH 8
#define DEFAULT_IPV4_HDR_LENGTH 20

#define GTP_V1_MASK 0x20
#define PT_MASK 0x10
#define GTP_E_MASK 0x04
#define GTP_S_MASK 0x02
#define GTP_PN_MASK 0x01

#define GTP_ECHO_REQUEST 1
#define GTP_ECHO_RESPONSE 2
#define GTP_ERROR_INDICATION 26
#define GTP_END_MARKER 254
#define G_PDU 255

#define PFCP_APPLY_ACTION_LEN 1
#define PFCP_APPLY_ACTION_DROP_MASK 0x01
#define PFCP_APPLY_ACTION_FORW_MASK 0x02
#define PFCP_APPLY_ACTION_BUFF_MASK 0x04
#define PFCP_APPLY_ACTION_NOCP_MASK 0x08

#define UPF_PDR_MASK_OHR (1 << 0)
#define UPF_PDR_MASK_QER (1 << 1)
#define UPF_PDR_MASK_FAR (1 << 2)
#define UPF_PDR_MASK_UPDATE (1 << 30)
#define UPF_PDR_MASK_NEW (1 << 31)
#define UPF_FAR_MASK_OHC (1 << 0)
#define UPF_FAR_MASK_BAR (1 << 1)

struct pdi_info_uplink {
  __u8 srcIface;
  __u32 ueIpAddr;
  __u32 teid;
  __u8 qfi;

  bool operator<(const pdi_info_uplink& rhs) const {
    if (srcIface != rhs.srcIface)
      return srcIface < rhs.srcIface;
    if (ueIpAddr != rhs.ueIpAddr)
      return ueIpAddr < rhs.ueIpAddr;
    if (teid != rhs.teid)
      return teid < rhs.teid;
    return qfi < rhs.qfi;
  }
};

struct pdi_info_downlink {
  __u8 srcIface;
  __u32 ueDestIpAddr;

  bool operator<(const pdi_info_downlink& rhs) const {
    if (srcIface != rhs.srcIface)
      return srcIface < rhs.srcIface;

    return ueDestIpAddr < rhs.ueDestIpAddr;
  }
};

typedef struct outerHdrRemoval {
  uint16_t IEI;
  /* length field, 2 bytes, will be filled by the library */

  uint8_t description;
  uint8_t gtpUextensionHdrDel;

} outerHdrRemoval_t;

typedef struct outHdrCreatn {
  uint16_t IEI;
  /* length field, 2 bytes, will be filled by the library */
  uint16_t description;
  uint32_t teid;
  uint8_t ipv4Addr[PFCP_IPV4_ADDR_LEN];
  uint8_t ipv6Addr[PFCP_IPV6_ADDR_LEN];
  uint16_t port;

} outHdrCreatn_t;

struct pdr_value {
  __u32 SEID;
  __u32 pdrMask; // UPF_PDR_MASK_OHR
  __u32 farMask; // UPF_FAR_MASK_OHC

  outerHdrRemoval_t ohr;
  outHdrCreatn_t ohc;

  __u8 applyAction;
  __u8 destIface;
};

void printPdiInfoUplink(const pdi_info_uplink& info);
void printPdiInfoUplink(const pdi_info_uplink& info);