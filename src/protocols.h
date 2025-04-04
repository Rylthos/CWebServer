#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <stdint.h>
#include <stdio.h>

#include <sys/socket.h>

extern uint32_t src_ip;
extern int port_number;

struct pseudoHeader {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint8_t fixed;
  uint8_t protocol;
  uint16_t segment_length;
};

struct tcpHeader {
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
};

static const int max_segment_size = 536;
static const int max_data_size = max_segment_size - sizeof(struct tcpHeader);

void printTCPSegment(char *segments, size_t segment_size);

char *createTCPSegments(const char *data, size_t data_length,
                        size_t *segment_size, struct sockaddr *dstAddr);
#endif
