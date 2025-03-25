#include "protocols.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint32_t src_ip;
int port_number;

// static const int max_segment_size = 536;
// static const int max_data_size = max_segment_size - sizeof(struct tcpHeader);

int checksum(char *data, size_t data_length, struct pseudoHeader *header) {
  uint16_t checksum = 0;
  checksum += header->src_ip >> 16;
  checksum += header->src_ip & 0xFFFF;
  checksum += header->dst_ip >> 16;
  checksum += header->dst_ip & 0xFFFF;
  checksum += ((uint16_t)header->fixed << 8) + header->protocol;
  checksum += header->segment_length;
  for (int i = 0; i < data_length; i += 2) {
    uint16_t v = ((uint16_t)data[i] << 8) + data[i + 1];
    checksum += v;
  }
  return ~checksum;
}

void printTCPSegments(char *segments, size_t segmentCount) {
  FILE *f;
  f = fopen("output.bin", "wb");
  for (int i = 0; i < segmentCount; i++) {
    char *currentSegment = segments + (i * max_segment_size);

    struct tcpHeader *header = (struct tcpHeader *)currentSegment;

    printf("******************** SEGM %3d ********************\n", i);
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

    const int max_length = 16;
    char store[max_length];
    for (int i = 0; i < max_data_size; i++) {
      if (i != 0 && i % max_length == 0) {
        printf("\t%.*s\n\t", max_length, store);
      }

      char data = *(currentSegment + sizeof(struct tcpHeader) + i);
      store[i % max_length] = data;
      if (data < 32)
        store[i % max_length] = '.';

      printf("%02x ", data);
    }
    printf("\n******************** SEGM %3d ********************\n", i);
  }
  fwrite(segments, segmentCount * max_segment_size, 1, f);
  fclose(f);
}

char *createTCPSegments(const char *data, size_t data_length,
                        size_t *segmentCount, struct sockaddr *dstAddr) {
  *segmentCount = data_length / max_data_size + 1;

  char *segments = malloc(*segmentCount * max_segment_size);

  memset(segments, 0, *segmentCount * max_segment_size);

  struct pseudoHeader ipHeader;
  ipHeader.src_ip = src_ip;
  ipHeader.dst_ip = ((struct sockaddr_in *)dstAddr)->sin_addr.s_addr;
  ipHeader.protocol = IPPROTO_TCP;
  ipHeader.segment_length = max_segment_size;

  struct tcpHeader header;
  memset(&header, 0, sizeof(header));

  header.src_port = htons(port_number);
  header.dst_port = ((struct sockaddr_in *)dstAddr)->sin_port;
  header.data_offset = 5;
  header.window = htons(5840);

  for (int i = 0; i < *segmentCount; i++) {
    const char *data_pos = data + (i * max_data_size);
    char *segment = segments + i * max_segment_size;

    int length = max_data_size;
    if ((data_pos + length) > (data + data_length)) {
      length = (data + data_length) - (data_pos);
    }

    memcpy(segment + sizeof(struct tcpHeader), data, length);

    header.seq_num = htonl(i * max_data_size);

    if (i == 0) {
      header.SYN = 1;
    } else {
      header.SYN = 0;
    }

    if (i == *segmentCount - 1) {
      header.FIN = 1;
    } else {
      header.FIN = 0;
    }

    memcpy(segment, &header, sizeof(struct tcpHeader));
    header.checksum = checksum(segment, max_segment_size, &ipHeader);
    memcpy(segment, &header, sizeof(struct tcpHeader));
  }

  return segments;
}
