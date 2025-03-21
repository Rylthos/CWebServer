#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "response.h"

int socketFD;

void cleanup_socket()
{
    close(socketFD);

    printf("Closed socket\n");

    cleanup();
}

void int_handler(int v)
{
    printf("\n\nENDING\n");
    cleanup_socket();
    exit(-2);
}

int main(int argc, char** argv)
{
    signal(SIGINT, int_handler);

    if (argc != 4) {
        printf("Incorrect Usage: web <addr> <port> <file>\n");
        return -1;
    }

    const char* addrLoc = argv[1];
    int port = atoi(argv[2]);
    char* sourceLoc = argv[3];

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (!socketFD) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        exit(-1);
    }
    printf("Started socket\n");

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { .s_addr = inet_addr(addrLoc), },
    };

    if (bind(socketFD, (struct sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
        exit(-1);
    };
    printf("Bound socket at %s:%d\n", addrLoc, port);

    setup(sourceLoc);

    size_t readBufLength = 500;
    char* readBuf = malloc(readBufLength + 1);
    readBuf[readBufLength] = 0;

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = 0;
        if (listen(socketFD, 50) == -1) {
            fprintf(stderr, "Failed to listen\n");

            cleanup();
            exit(-1);
        }

        int clientSocket = accept(socketFD, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (!clientSocket) {
            fprintf(stderr, "Failed to accept connection\n");
            continue;
        }
        printf("Connected\n");

        size_t totalRead = 0;
        ssize_t msgLength = 0;

        do {
            if (msgLength == readBufLength) {
                size_t newLength = readBufLength * 2;
                char* newData = malloc(newLength + 1);
                memcpy(newData, readBuf, readBufLength);
                readBufLength = newLength;
                free(readBuf);
                readBuf = newData;
                readBuf[newLength] = 0;
            }

            msgLength = recv(
                clientSocket, readBuf + totalRead, (readBufLength - totalRead) * sizeof(char), 0);
            totalRead += msgLength;
        } while (msgLength == readBufLength);

        handle_msg(clientSocket, readBuf, totalRead);

        close(clientSocket);
        printf("Closed socket\n");
    }

    cleanup_socket();
}
