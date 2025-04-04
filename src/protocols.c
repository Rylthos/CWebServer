#include "protocols.h"

#include <arpa/inet.h>
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

uint16_t checksum(const uint8_t *data, size_t data_length) {
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

void printIPPacket(const uint8_t *packet, size_t packet_size) {
  IPHeader *header = (IPHeader *)packet;

  printf("********************  IP  ********************\n");
  printf("Version         : %d\n", header->version);
  printf("IHL             : %d\n", header->IHL);
  printf("TOS             : %d\n", header->tos);
  printf("Total Length    : %u\n", ntohs(header->total_length));
  printf("ID              : %u\n", ntohs(header->id));
  printf("Fragment Offset : %d\n", ntohs(header->fragment_offset));
  printf("TTL             : %d\n", header->ttl);
  printf("Protocol        : %d\n", header->protocol);
  printf("Checksum        : 0x%04x\n", header->checksum);
  printf("Src Addr        : %s\n",
         inet_ntoa(*(struct in_addr *)&header->src_addr));
  printf("Dst Addr        : %s\n",
         inet_ntoa(*(struct in_addr *)&header->dst_addr));

  uint8_t *segment;
  uint32_t segment_size;
  getTCPSegment(packet, packet_size, &segment, &segment_size);

  printTCPSegment(segment, segment_size);
  printf("********************  IP  ********************\n");
}

void printTCPSegment(const uint8_t *segment, size_t segment_size) {
  TCPHeader *header = (TCPHeader *)segment;

  printf("******************** TCP ********************\n");
  printf("src_port   : %5d\n", ntohs(header->src_port));
  printf("dst_port   : %5d\n", ntohs(header->dst_port));
  printf("seq_num    : %5u\n", ntohl(header->seq_num));
  printf("ack_num    : %5u\n", ntohl(header->ack_num));
  printf("data_offset: %5d\n", header->data_offset);
  printf("flags      |\n");
  printf("           |- CWR: %d\n", header->flags.CWR);
  printf("           |- ECE: %d\n", header->flags.ECE);
  printf("           |- URG: %d\n", header->flags.URG);
  printf("           |- ACK: %d\n", header->flags.ACK);
  printf("           |- PSH: %d\n", header->flags.PSH);
  printf("           |- RST: %d\n", header->flags.RST);
  printf("           |- SYN: %d\n", header->flags.SYN);
  printf("           |- FIN: %d\n", header->flags.FIN);
  printf("window     : %d\n", ntohs(header->window));
  printf("checksum   : 0x%4x\n", ntohs(header->checksum));
  printf("urgent_ptr : %d\n", ntohs(header->urgent_ptr));
  printf("data       :\n\t");

  uint8_t *data;
  uint32_t data_length;
  getTCPDataSegment(segment, segment_size, &data, &data_length);
  print_hex(data, data_length, "\t");
  printf("RAW\n");
  print_hex((uint8_t *)segment, segment_size, "");

  printf("******************** TCP ********************\n");
}

void getTCPSegment(const uint8_t *packet, size_t packet_size, uint8_t **segment,
                   uint32_t *segment_size) {
  IPHeader *ip_hdr = (IPHeader *)packet;
  uint32_t ip_header_size = ip_hdr->IHL * 4;
  TCPHeader *header = (TCPHeader *)(packet + ip_header_size);

  if (segment != NULL) {
    *segment = (uint8_t *)(packet + ip_header_size);
  }

  if (segment_size != NULL) {
    *segment_size = packet_size - ip_header_size;
  }
}

void getTCPDataPacket(const uint8_t *packet, size_t packet_size, uint8_t **data,
                      uint32_t *data_size) {
  uint8_t *segment;
  uint32_t segment_size;
  getTCPSegment(packet, packet_size, &segment, &segment_size);

  if (data != NULL) {
    *data = segment + sizeof(TCPHeader);
  }

  if (data_size != NULL) {
    *data_size = segment_size - sizeof(TCPHeader);
  }
}

void getTCPDataSegment(const uint8_t *segment, size_t segment_size,
                       uint8_t **data, uint32_t *data_size) {
  if (data != NULL) {
    *data = (uint8_t *)segment + sizeof(TCPHeader);
  }

  if (data_size != NULL) {
    *data_size = segment_size - sizeof(TCPHeader);
  }
}

PacketType getTCPPacketType(uint8_t *packet) {
  TCPHeader *header = (TCPHeader *)(packet + sizeof(IPHeader));

  if (header->flags.SYN) {
    return SYN;
  } else if (header->flags.FIN) {
    return FIN;
  } else if (header->flags.PSH) {
    return PSH;
  } else if (header->flags.ACK) {
    return ACK;
  }

  return NONE;
}

void createGenericPacket(struct sockaddr_in *src_addr,
                         struct sockaddr_in *dst_addr, int32_t seq_num,
                         int32_t ack_seq, uint8_t *data, uint32_t data_length,
                         uint8_t **packet, uint32_t *packet_len,
                         TCPFlags flags) {
  size_t total_size = sizeof(IPHeader) + sizeof(TCPHeader) + data_length;
  uint8_t *datagram = calloc(total_size, sizeof(uint8_t));

  IPHeader *ipHeader = (IPHeader *)datagram;
  TCPHeader *tcpHeader = (TCPHeader *)(datagram + sizeof(IPHeader));
  PseudoIPHeader pseudoHeader;

  uint8_t *payload = datagram + sizeof(IPHeader) + sizeof(TCPHeader);
  if (data_length != 0) {
    memcpy(payload, data, data_length);
  }

  ipHeader->IHL = 5;
  ipHeader->version = 4;
  ipHeader->tos = 0;
  ipHeader->total_length = total_size;
  ipHeader->id = htonl(rand() % 65535);
  ipHeader->fragment_offset = 0;
  ipHeader->ttl = 64;
  ipHeader->protocol = IPPROTO_TCP;
  ipHeader->checksum = 0;
  ipHeader->src_addr = src_addr->sin_addr.s_addr;
  ipHeader->dst_addr = dst_addr->sin_addr.s_addr;

  tcpHeader->src_port = src_addr->sin_port;
  tcpHeader->dst_port = dst_addr->sin_port;
  tcpHeader->seq_num = htonl(seq_num);
  tcpHeader->ack_num = htonl(ack_seq);
  tcpHeader->data_offset = 5;
  tcpHeader->flags = flags;
  tcpHeader->checksum = 0;
  tcpHeader->window = htons(5840);
  tcpHeader->urgent_ptr = 0;

  pseudoHeader.src_ip = src_addr->sin_addr.s_addr;
  pseudoHeader.dst_ip = dst_addr->sin_addr.s_addr;
  pseudoHeader.fixed = 0;
  pseudoHeader.protocol = IPPROTO_TCP;
  pseudoHeader.segment_length = htons(sizeof(TCPHeader) + data_length);

  size_t pseudogram_size =
      sizeof(PseudoIPHeader) + sizeof(TCPHeader) + data_length;
  uint8_t *pseudogram = malloc(pseudogram_size);

  memcpy(pseudogram, &pseudoHeader, sizeof(PseudoIPHeader));
  memcpy(pseudogram + sizeof(PseudoIPHeader), tcpHeader,
         sizeof(TCPHeader) + data_length);

  tcpHeader->checksum = checksum(pseudogram, pseudogram_size);
  ipHeader->checksum = checksum(datagram, total_size);

  *packet = datagram;
  *packet_len = total_size;

  free(pseudogram);
}

void createSynAckPacket(struct sockaddr_in *src_addr,
                        struct sockaddr_in *dst_addr, int32_t seq_num,
                        int32_t ack_seq, uint8_t **packet,
                        uint32_t *packet_len) {

  TCPFlags flags = {
      .FIN = 0,
      .SYN = 1,
      .RST = 0,
      .PSH = 0,
      .ACK = 1,
      .URG = 0,
  };

  createGenericPacket(src_addr, dst_addr, seq_num, ack_seq, NULL, 0, packet,
                      packet_len, flags);
}

void createAckPacket(struct sockaddr_in *src_addr, struct sockaddr_in *dst_addr,
                     int32_t seq_num, int32_t ack_seq, uint8_t **packet,
                     uint32_t *packet_len) {
  TCPFlags flags = {
      .FIN = 0,
      .SYN = 0,
      .RST = 0,
      .PSH = 0,
      .ACK = 1,
      .URG = 0,
  };

  createGenericPacket(src_addr, dst_addr, seq_num, ack_seq, NULL, 0, packet,
                      packet_len, flags);
}

void createDataPacket(struct sockaddr_in *src_addr,
                      struct sockaddr_in *dst_addr, int32_t seq_num,
                      int32_t ack_seq, uint8_t *data, uint32_t data_length,
                      uint8_t **packet, uint32_t *packet_len) {
  TCPFlags flags = {
      .FIN = 0,
      .SYN = 0,
      .RST = 0,
      .PSH = 1,
      .ACK = 1,
      .URG = 0,
  };

  createGenericPacket(src_addr, dst_addr, seq_num, ack_seq, data, data_length,
                      packet, packet_len, flags);
}

void createFinAckPacket(struct sockaddr_in *src_addr,
                        struct sockaddr_in *dst_addr, int32_t seq_num,
                        int32_t ack_seq, uint8_t **packet,
                        uint32_t *packet_len) {
  TCPFlags flags = {
      .FIN = 1,
      .SYN = 0,
      .RST = 0,
      .PSH = 0,
      .ACK = 1,
      .URG = 0,
  };

  createGenericPacket(src_addr, dst_addr, seq_num, ack_seq, NULL, 0, packet,
                      packet_len, flags);
}
