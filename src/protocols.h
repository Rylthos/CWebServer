#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <stdint.h>
#include <stdio.h>

#include <netinet/in.h>
#include <sys/socket.h>

#define DATAGRAM_SIZE 4096

extern uint32_t src_ip;
extern int port_number;

typedef struct pseudoIpHeader {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint8_t fixed;
  uint8_t protocol;
  uint16_t segment_length;
} PseudoIPHeader;

typedef struct ipHeader {
  uint8_t IHL : 4;
  uint8_t version : 4;
  uint8_t tos;
  uint16_t total_length;
  uint16_t id;
  uint16_t fragment_offset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t src_addr;
  uint32_t dst_addr;
} IPHeader;

typedef struct tcpHeader {
  uint16_t src_port;
  uint16_t dst_port;
  uint32_t seq_num;
  uint32_t ack_num;

  union {
    uint8_t data;
    struct {
      uint8_t _ : 4;
      uint8_t data_offset : 4;
    };
  };

  union {
    uint8_t flags;
    struct {
      uint8_t FIN : 1;
      uint8_t SYN : 1;
      uint8_t RST : 1;
      uint8_t PSH : 1;
      uint8_t ACK : 1;
      uint8_t URG : 1;
      uint8_t ECE : 1;
      uint8_t CWR : 1;
    };
  };

  uint16_t window;
  uint16_t checksum;
  uint16_t urgent_ptr;
} TCPHeader;

uint16_t checksum(const uint8_t *data, size_t data_length);

typedef enum packetType {
  NONE,
  ACK,
  SYN,
  PSH,
  FIN,
} PacketType;

void printIPPacket(const uint8_t *packet, size_t packet_size);
void printTCPSegment(const uint8_t *segment, size_t segment_size);

PacketType getTCPPacketType(uint8_t *packet);

void createSynAckPacket(struct sockaddr_in *src_addr,
                        struct sockaddr_in *dst_addr, int32_t seq_num,
                        int32_t ack_seq, uint8_t **packet,
                        uint32_t *packet_len);

void createAckPacket(struct sockaddr_in *src_addr, struct sockaddr_in *dst_addr,
                     int32_t seq_num, int32_t ack_seq, uint8_t **packet,
                     uint32_t *packet_len);

void createDataPacket(struct sockaddr_in *src_addr,
                      struct sockaddr_in *dst_addr, int32_t seq_num,
                      int32_t ack_seq, uint8_t *data, uint32_t data_length,
                      uint8_t **packet, uint32_t *packet_len);

void createFinAckPacket(struct sockaddr_in *src_addr,
                        struct sockaddr_in *dst_addr, int32_t seq_num,
                        int32_t ack_seq, uint8_t **packet,
                        uint32_t *packet_len);
#endif
