#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
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

int sendFD;
int recvFD;

void cleanup_socket() {
  close(sendFD);
  close(recvFD);

  LOG_GENERAL("Closed Socket\n");
  // printf("Closed socket\n");

  cleanup();
}

void int_handler(int v) {
  LOG_GENERAL("\n\nEnding\n");
  cleanup_socket();
  exit(-2);
}

int main(int argc, char **argv) {
  signal(SIGINT, int_handler);

  if (argc != 4) {
    printf("Incorrect Usage: web <addr> <port> <file>\n");
    return -1;
  }

  const char *addrLoc = argv[1];
  int port = atoi(argv[2]);
  char *sourceLoc = argv[3];

  // TCP Header must be created. IP header created for us
  sendFD = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  if (sendFD == -1) {
    fprintf(stderr, "Failed to create socket: %s | %d\n", strerror(errno),
            errno);
    return -1;
  }
  LOG_GENERAL("Created send socket: %d\n", sendFD);

  recvFD = socket(AF_INET, SOCK_STREAM, 0);
  if (recvFD == -1) {
    fprintf(stderr, "Failed to created receive socket: %s | %d",
            strerror(errno), errno);
    return -1;
  }
  LOG_GENERAL("Created receive socket: %d\n", recvFD);

  port_number = port;
  src_ip = inet_addr(addrLoc);

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr.s_addr = inet_addr(addrLoc),
  };

  if (bind(sendFD, (struct sockaddr *)&addr, sizeof(addr))) {
    fprintf(stderr, "Failed to bind socket: %d : %s\n", sendFD,
            strerror(errno));
    exit(-1);
  };

  addr.sin_port = htons(port);
  if (bind(recvFD, (struct sockaddr *)&addr, sizeof(addr))) {
    fprintf(stderr, "Failed to bind socket: %d : %s\n", recvFD,
            strerror(errno));
    exit(-1);
  };
  LOG_GENERAL("Bound socket at %s:%d\n", addrLoc, port);

  setup(sourceLoc);

  size_t readBufLength = 500;
  char *readBuf = malloc(readBufLength + 1);
  readBuf[readBufLength] = 0;

  struct sockaddr *clientAddr = malloc(sizeof(struct sockaddr_in));
  socklen_t clientAddrSize;

  while (1) {
    clientAddrSize = sizeof(struct sockaddr_in);
    if (listen(recvFD, 50) == -1) {
      fprintf(stderr, "Failed to listen: %s | %d\n", strerror(errno), errno);

      cleanup();
      exit(-1);
    }

    int clientSocket = accept(recvFD, clientAddr, &clientAddrSize);

    if (!clientSocket) {
      fprintf(stderr, "Failed to accept connection\n");
      continue;
    }
    {
      struct sockaddr_in *data = (struct sockaddr_in *)clientAddr;
      LOG_GENERAL("Connected to | %s:%d\n", inet_ntoa(data->sin_addr),
                  ntohs(data->sin_port));
    }

    size_t totalRead = 0;
    ssize_t msgLength = 0;

    do {
      if (msgLength == readBufLength) {
        size_t newLength = readBufLength * 2;
        char *newData = malloc(newLength + 1);
        memcpy(newData, readBuf, readBufLength);
        readBufLength = newLength;
        free(readBuf);
        readBuf = newData;
        readBuf[newLength] = 0;
      }

      msgLength = recv(clientSocket, readBuf + totalRead,
                       (readBufLength - totalRead) * sizeof(char), 0);

      totalRead += msgLength;
    } while (msgLength == readBufLength);

    handle_msg(clientAddr, clientAddrSize, sendFD, readBuf, totalRead);

    shutdown(clientSocket, SHUT_WR);
    // close(clientSocket);
    LOG_GENERAL("Closed socket\n");
  }

  cleanup_socket();
}
