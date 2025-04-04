#include "protocols.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

uint32_t src_ip;
int port_number;

uint16_t checksum(const char *data, size_t data_length) {
  uint32_t sum = 0, i;
  for (i = 0; i < data_length - 1; i += 2) {
    uint16_t v = *(uint16_t *)&data[i];
    sum += v;
  }

  if (data_length & 1) {
    uint16_t v = (uint8_t)data[i];
    sum += v;
  }

  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return ~sum;
}

void printTCPSegment(char *segment, size_t segment_size) {
  FILE *f;
  f = fopen("output.bin", "wb");
  char *currentSegment = segment;

  struct tcpHeader *header = (struct tcpHeader *)currentSegment;

  printf("******************** SEGM ********************\n");
  printf("src_port   : %5d\n", ntohs(header->src_port));
  printf("dst_port   : %5d\n", ntohs(header->dst_port));
  printf("seq_num    : %5d\n", ntohl(header->seq_num));
  printf("data_offset: %5d\n", header->data_offset);
  printf("flags      |\n");
  printf("           |- CWR: %d\n", header->CWR);
  printf("           |- ECE: %d\n", header->ECE);
  printf("           |- URG: %d\n", header->URG);
  printf("           |- ACK: %d\n", header->ACK);
  printf("           |- PSH: %d\n", header->PSH);
  printf("           |- RST: %d\n", header->RST);
  printf("           |- SYN: %d\n", header->SYN);
  printf("           |- FIN: %d\n", header->FIN);
  printf("window     : %d\n", ntohs(header->window));
  printf("checksum   : 0x%4x\n", ntohs(header->checksum));
  printf("urgent_ptr : %d\n", ntohs(header->urgent_ptr));
  printf("data       :\n\t");

  print_hex((uint8_t *)currentSegment + sizeof(struct tcpHeader), max_data_size,
            "\t");
  printf("RAW\n");
  print_hex((uint8_t *)currentSegment, max_segment_size, "");

  printf("******************** SEGM ********************\n");
  fwrite(segment, segment_size, 1, f);
  fclose(f);
}

char *createTCPSegments(const char *data, size_t data_length,
                        size_t *segment_size, struct sockaddr *dstAddr) {
  // *segmentCount = data_length / max_data_size + 1;
  char *segment = calloc(data_length + sizeof(struct tcpHeader), sizeof(char));
  *segment_size = data_length + sizeof(struct tcpHeader);

  struct tcpHeader *header = (struct tcpHeader *)segment;
  char *data_loc = segment + sizeof(struct tcpHeader);

  memcpy(data_loc, data, data_length);

  struct pseudoHeader ipHeader;
  ipHeader.src_ip = src_ip;
  ipHeader.dst_ip = ((struct sockaddr_in *)dstAddr)->sin_addr.s_addr;
  ipHeader.fixed = 0;
  ipHeader.protocol = IPPROTO_TCP;
  ipHeader.segment_length = htons(data_length + sizeof(struct tcpHeader));

  header->src_port = htons(port_number);
  header->dst_port = ((struct sockaddr_in *)dstAddr)->sin_port;
  header->data_offset = 5;
  header->window = htons(5840);
  header->seq_num = 1;
  header->ack_num = 350;
  header->PSH = 1;
  header->ACK = 1;
  header->checksum = 0;

  size_t pseudogram_size =
      sizeof(struct pseudoHeader) + sizeof(struct tcpHeader) + data_length;
  char *pseudogram = calloc(pseudogram_size, sizeof(char));

  memcpy(pseudogram, (char *)&ipHeader, sizeof(struct pseudoHeader));

  for (int i = 0; i < sizeof(struct tcpHeader) + data_length; i++) {
    *(pseudogram + sizeof(struct pseudoHeader) + i) = segment[i];
  }

  header->checksum = checksum(pseudogram, pseudogram_size);

  free(pseudogram);

  return segment;
}
