#ifndef PACKET_H
#define  PACKET_H

#include<cstdint>

#define DATA 0x01  //Flag for data packet
#define ACK 0x02   //Flag for ACK packet 
#define SYN 0x04   //Flag for SYN packet
#define FIN 0x08   //Flag for FIN packet
#define MAX_PAYLOAD 1024 //Setting default Max payload size to 1024 characters

#pragma pack(1) //To prevent to compiler form adding padding

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

uint16_t calculate_checksum(void* vdata, size_t length){
    char *data = (char*)vdata;
    uint32_t acc = 0xffff;

    for(size_t i = 0 ; i<length ; i += 2){// Adding one word at a time
        uint16_t word = 0;
        memcpy(&word, data+i, 2);
        acc += ntohs(word);

        if(acc > 0xffff){
            acc -= 0xffff;
        }
    }

    if(length & 1){//Accounting for the last byte if the length is odd
        uint16_t word = 0;
        memcpy(&word, data + length -1 , 1);

        acc += word;

        if(acc > 0xffff){
            acc -= 0xffff;
        }
    }

    return (~acc);
}

#endif
