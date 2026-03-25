#ifndef PACKET_H
#define  PACKET_H

#include<cstdint>
#include<iostream>
#include<cstring>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<cstdio>

#define DATA 0x01  //Flag for data packet
#define ACK 0x02   //Flag for ACK packet 
#define SYN 0x04   //Flag for SYN packet
#define FIN 0x08   //Flag for FIN packet
#define MAX_PAYLOAD 1024 //Setting default Max payload size to 1024 characters

#pragma pack(push, 1) //To prevent to compiler form adding padding

struct Header{  //Defining a Header structure for packets
    uint32_t seq_num; //Sequence Number
    uint32_t ack_num; //Acknowledgment Number
    uint8_t flags;  //Flags for the packet
    uint16_t checksum;  //Checksum for error detection
};

#pragma pack(pop)

struct Packet{// A Packet contains the header and the payload
    Header header;
    char payload[MAX_PAYLOAD];
};

#endif
