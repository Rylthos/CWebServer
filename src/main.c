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
  socklen_t clientAddrSize = sizeof(clientAddr);

  ssize_t packet_size;
  uint8_t buffer[65535];
  int32_t current_port = -1;
  while (1) {
    packet_size = recvfrom(serverFD, buffer, 65535, 0,
                           (struct sockaddr *)&clientAddr, &clientAddrSize);

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

    uint32_t seq_num =
        ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->seq_num);
    uint32_t ack_seq =
        ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->ack_num);
    clientAddr.sin_port = ((TCPHeader *)(buffer + sizeof(IPHeader)))->src_port;

    current_port = clientAddr.sin_port;

    uint8_t *ack_packet;
    uint32_t ack_packet_size;

    uint32_t new_seq_num = seq_num + 1;
    createSynAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &ack_packet,
                       &ack_packet_size);

    int sent = sendto(serverFD, ack_packet, ack_packet_size, 0,
                      (struct sockaddr *)&clientAddr, clientAddrSize);
    LOG_SEND_HEADER_START;
    printIPPacket(ack_packet, ack_packet_size);
    printTCPSegment(ack_packet + sizeof(IPHeader),
                    ack_packet_size - sizeof(IPHeader));
    LOG_SEND_HEADER_END;

    if (sent == -1) {
      fprintf(stderr, "Replying ACK failed\n");
    } else {
      printf("Sent %d bytes ACK!\n", sent);
    }

    free(ack_packet);
    ack_packet = NULL;

    do {
      // Should be an ACK
      packet_size = recvfrom(serverFD, buffer, 65535, 0,
                             (struct sockaddr *)&clientAddr, &clientAddrSize);

      LOG_RECV_HEADER_START;
      printIPPacket(buffer, packet_size);
      printTCPSegment(buffer + sizeof(IPHeader), packet_size);
      LOG_RECV_HEADER_END;

      // Should be data
      packet_size = recvfrom(serverFD, buffer, 65535, 0,
                             (struct sockaddr *)&clientAddr, &clientAddrSize);

      LOG_RECV_HEADER_START;
      printIPPacket(buffer, packet_size);
      printTCPSegment(buffer + sizeof(IPHeader), packet_size);
      LOG_RECV_HEADER_END;

      seq_num = ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->seq_num);
      ack_seq = ntohl(((TCPHeader *)(buffer + sizeof(IPHeader)))->ack_num);
      clientAddr.sin_port =
          ((TCPHeader *)(buffer + sizeof(IPHeader)))->src_port;
    } while (clientAddr.sin_port != current_port);

    handle_msg(&addr, &clientAddr, ack_seq, new_seq_num, serverFD,
               buffer + sizeof(TCPHeader) + sizeof(IPHeader),
               packet_size - sizeof(TCPHeader) - sizeof(IPHeader));

    // Should be an ACK
    packet_size = recvfrom(serverFD, buffer, 65535, 0,
                           (struct sockaddr *)&clientAddr, &clientAddrSize);

    LOG_RECV_HEADER_START;
    printIPPacket(buffer, packet_size);
    printTCPSegment(buffer + sizeof(IPHeader), packet_size);
    LOG_RECV_HEADER_END;
  }

  cleanup_socket();
}
