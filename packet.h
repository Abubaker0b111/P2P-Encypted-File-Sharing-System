#ifndef PACKET_H
#define  PACKET_H

#include<cstdint>

#define DATA 0x01
#define ACK 0x02
#define SYN 0x04
#define FIN 0x08
#define MAX_PAYLOAD 1024

#pragma pack(1)

struct Header{
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t flags;
    uint16_t checksum;
};

#pragma pack(pop)

struct Packet{
    Header header;
    char payload[MAX_PAYLOAD];
};

#endif
