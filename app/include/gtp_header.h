#ifndef _PARSING_HELPER_H_
#define _PARSING_HELPER_H_

#include <unet.h>


enum extHdrType
{
    E_GTP_EXT_HDR_NO_MORE_EXTENSION_HEADERS = 0b00000000,
    E_GTP_EXT_HDR_SERVICE_CLASS_INDICATOR = 0b00100000,
    E_GTP_EXT_HDR_UDP_PORT = 0b01000000,
    E_GTP_EXT_HDR_RAN_CONTAINER = 0b10000001,
    E_GTP_EXT_HDR_LONG_PDCP_PDU_NUMBER = 0b10000010,
    E_GTP_EXT_HDR_XW_RAN_CONTAINER = 0b10000011,
    E_GTP_EXT_HDR_NR_RAN_CONTAINER = 0b10000100,
    E_GTP_EXT_HDR_PDU_SESSION_CONTAINER = 0b10000101,
    E_GTP_EXT_HDR_PDCP_PDU_NUMBER = 0b11000000
};

struct gtpheader
{
    uint8_t flags;
    uint8_t msgType;
    uint16_t length;
    uint32_t teid;
    uint16_t seqNo;
    uint8_t npduNum;
    uint8_t nxtExtnHdrType;
};

struct pdu_ul_extension_header
{
    uint8_t len;
    uint8_t : 4;
    uint8_t pduType : 4;
    uint8_t qfi : 6;
    uint8_t : 2;
    uint8_t nxtExtnHdrType;
};

struct pdu_dl_extension_header
{
    uint8_t len;
    uint8_t : 4;
    uint8_t pduType : 4;
    uint8_t qfi : 6;
    uint8_t rqi : 1;
    uint8_t : 1;
    uint8_t nxtExtnHdrType;
};

struct pdu_extension_header
{
    union
    {
        struct pdu_ul_extension_header *uplink_extension_header;
        struct pdu_dl_extension_header *downlink_extension_header;
    };
    int pdu_type;
};

#endif