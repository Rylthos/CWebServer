#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "protocols.h"
#include "response.h"

int serverFD;

void cleanup_socket() {
  close(serverFD);

  LOG_GENERAL("Closed Socket\n");

  cleanup();
}

void int_handler(int v) {
  LOG_GENERAL("\n\nEnding\n");
  cleanup_socket();
  exit(-2);
}

void getSeqAckPortIP(uint8_t *buffer, uint32_t *seq_num, uint32_t *ack_seq,
                     uint16_t *port, uint32_t *ip) {
  *seq_num = ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->seq_num);
  *ack_seq = ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->ack_num);
  *port = ((TCPHeader *)(buffer + sizeof(IPHeader)))->src_port;
  *ip = ((IPHeader *)buffer)->src_addr;
}

int main(int argc, char **argv) {
  signal(SIGINT, int_handler);

  assert(sizeof(TCPHeader) == sizeof(struct tcphdr) && "Matching TCP size");

  if (argc != 4) {
    printf("Incorrect Usage: web <addr> <port> <file>\n");
    return -1;
  }

  const char *addrLoc = argv[1];
  int port = atoi(argv[2]);
  char *sourceLoc = argv[3];

  // TCP Header must be created. IP header created for us
  serverFD = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  if (serverFD == -1) {
    fprintf(stderr, "Failed to create socket: %s | %d\n", strerror(errno),
            errno);
    return -1;
  }
  LOG_GENERAL("Created send socket: %d\n", serverFD);

  int one = 1;
  const int *val = &one;
  if (setsockopt(serverFD, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1) {
    fprintf(stderr, "setsockopt failed\n");
    exit(-1);
  }

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr.s_addr = inet_addr(addrLoc),
  };

  addr.sin_port = htons(port);
  if (bind(serverFD, (struct sockaddr *)&addr, sizeof(addr))) {
    fprintf(stderr, "Failed to bind socket: %d : %s\n", serverFD,
            strerror(errno));

    exit(-1);
  };
  LOG_GENERAL("Bound socket at %s:%d\n", addrLoc, port);

  setup(sourceLoc);

  size_t readBufLength = 500;
  char *readBuf = malloc(readBufLength + 1);
  readBuf[readBufLength] = 0;

  struct sockaddr_in clientAddr;

  ssize_t packet_size;
  uint8_t buffer[65535];
  int32_t current_port = -1;

  uint32_t seq_num, ack_seq, ip;
  uint16_t port_number;

  while (1) {
    packet_size = recvfrom(serverFD, buffer, 65535, 0, NULL, NULL);

    if (packet_size == -1) {
      fprintf(stderr, "Failed to get packet\n");
      cleanup();
      exit(1);
    } else {
      printf("Received %ld bytes\n", packet_size);
    }

    LOG_RECV_HEADER_START;
    printIPPacket(buffer, packet_size);
    printTCPSegment(buffer + sizeof(IPHeader), packet_size - sizeof(IPHeader));
    LOG_RECV_HEADER_END;

    getSeqAckPortIP(buffer, &seq_num, &ack_seq, &port_number, &ip);

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = port_number;
    clientAddr.sin_addr.s_addr = ip;

    PacketType type = getTCPPacketType(buffer);

    switch (type) {
    case SYN: // Respond with SYN ACK
    {
      printf("RECIEVED: SYN\n");
      uint8_t *packet;
      uint32_t packet_size;

      int new_seq_num = seq_num + 1;
      createSynAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet,
                         &packet_size);

      int sent = sendto(serverFD, packet, packet_size, 0,
                        (struct sockaddr *)&clientAddr, sizeof(clientAddr));

      if (sent == -1) {
        printf("Failed to send bytes\n");
      } else {
        printf("Sent %d bytes. SYN ACK\n", sent);
      }

      free(packet);

      break;
    }
    case PSH: {
      printf("RECIEVED: PSH\n");

      uint8_t *packet;
      uint32_t packet_size;

      int new_seq_num = seq_num + 1;
      createAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet,
                      &packet_size);

      int sent = sendto(serverFD, packet, packet_size, 0,
                        (struct sockaddr *)&clientAddr, sizeof(clientAddr));

      if (sent == -1) {
        printf("Failed to send bytes\n");
      } else {
        printf("Sent %d bytes. ACK\n", sent);
      }

      uint8_t *offset_buffer = buffer + sizeof(TCPHeader) + sizeof(IPHeader);
      int32_t buffer_size = packet_size - sizeof(TCPHeader) - sizeof(IPHeader);

      handle_msg(&addr, &clientAddr, ack_seq, new_seq_num, serverFD,
                 offset_buffer, buffer_size);

      createFinAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet,
                         &packet_size);

      sent = sendto(serverFD, packet, packet_size, 0,
                    (struct sockaddr *)&clientAddr, sizeof(clientAddr));

      if (sent == -1) {
        printf("Failed to send bytes\n");
      } else {
        printf("Sent %d bytes. ACK\n", sent);
      }
      break;
      break;
    }
    case FIN: {
      uint8_t *packet;
      uint32_t packet_size;

      int new_seq_num = seq_num + 1;
      createAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet,
                      &packet_size);

      int sent = sendto(serverFD, packet, packet_size, 0,
                        (struct sockaddr *)&clientAddr, sizeof(clientAddr));

      if (sent == -1) {
        printf("Failed to send bytes\n");
      } else {
        printf("Sent %d bytes. ACK\n", sent);
      }

      createFinAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet,
                         &packet_size);

      sent = sendto(serverFD, packet, packet_size, 0,
                    (struct sockaddr *)&clientAddr, sizeof(clientAddr));

      if (sent == -1) {
        printf("Failed to send bytes\n");
      } else {
        printf("Sent %d bytes. ACK\n", sent);
      }
      break;
    }
    case ACK:
      printf("RECIEVED: ACK\n");
      break;
    default:
      fprintf(stderr, "Unhandled type");
    }
  }

  cleanup_socket();
}
