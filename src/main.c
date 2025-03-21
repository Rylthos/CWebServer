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

int socketFD;

const char* errorString = "HTTP/1.1 404 Not Found\n";
const char* htmlHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
const char* pngHeaderString = "HTTP/1.1 200 OK\nContent-Type: images/png\n\n";
const char* jpgHeaderString = "HTTP/1.1 200 OK\nContent-Type: images/jpeg\n\n";
const char* svgHeaderString = "HTTP/1.1 200 OK\nContent-Type: image/svg+xml\n\n";
const char* cssHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/css\n\n";
const char* jsHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n";

regex_t getRegex;

char* sourceLoc;

void setup_regex()
{
    int retV
        = regcomp(&getRegex, "GET \\([[:alnum:][:punct:]]*\\) HTTP/[[:digit:]].[[:digit:]]", 0);
    if (retV) {
        fprintf(stderr, "Failed to create regex\n");
    }
}

void free_regex() { regfree(&getRegex); }

void send_file(int fd, char* data, size_t length)
{
    FILE* file;

    const char** header;

    char fileBuf[500];
    if (!strncmp(data, "/", length)) {
        sprintf(fileBuf, "%s/index.html", sourceLoc);
        file = fopen(fileBuf, "r");

        if (file == NULL) {
            fprintf(stderr, "Failed to open file: resources/index.html\n");
            return;
        }

        header = &htmlHeaderString;
    } else {
        sprintf(fileBuf, "%s/%.*s", sourceLoc, (int)length - 1, data + 1);

        switch (data[length - 1]) {
        case 'l': // HTML
            header = &htmlHeaderString;
            file = fopen(fileBuf, "r");
            break;
        case 'g': // JPG, PNG
            switch (data[length - 2]) {
            case 'n': // PNG
                header = &pngHeaderString;
                break;
            case 'p': // JPG
                header = &jpgHeaderString;
                break;
            case 'v': // SVG
                header = &svgHeaderString;
                break;
            }
            file = fopen(fileBuf, "rb");
            break;
        case 's': // CSS, JS, TS
            switch (data[length - 2]) {
            case 's': // CSS
                header = &cssHeaderString;
                break;
            case 'j': // JS
            case 't': // TS
                header = &jsHeaderString;
                break;
            }
            file = fopen(fileBuf, "r");
            break;
        default:
            fprintf(stderr, "Unkonwn file: %.*s\n", (int)length, data);
            write(fd, errorString, strlen(errorString));
            return;
        }
    }

    if (file == NULL) {
        printf("File doesnt exist: %.*s\n", (int)length, data);
        printf("Reply:\n===========\n%.*s==========\n", (int)strlen(errorString), errorString);
        write(fd, errorString, strlen(errorString));
        return;
    }

    printf("GET %s\n", fileBuf);

    fseek(file, 0, SEEK_END);

    size_t totalLength = ftell(file) + strlen(*header);
    fseek(file, 0, SEEK_SET);

    char* buf = malloc(totalLength * sizeof(char) + 1);
    memcpy(buf, *header, strlen(*header));
    fread(buf + strlen(*header), 1, totalLength - strlen(*header), file);
    buf[totalLength] = 0;

    printf("Reply:\n===========\n%.*s==========\n", (int)totalLength, buf);
    write(fd, buf, totalLength);

    free(buf);
}

void handle_get(int fd, char* data, size_t length)
{
    if (data[0] == '/') {
        send_file(fd, data, length);
    }
}

void parse_buf(int fd, char* data, size_t length)
{
    printf("Received from %d:\n=====\n%.*s=====\n", fd, (int)length, data);

    regmatch_t* match = malloc((getRegex.re_nsub + 1) * sizeof(regmatch_t));
    int retV = regexec(&getRegex, data, getRegex.re_nsub + 1, match, 0);
    if (!retV) { // handle get Request
        printf("GET %.*s\n", match[1].rm_eo - match[1].rm_so, data + match[1].rm_so);

        handle_get(fd, data + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
    }

    free(match);
}

void cleanup()
{
    close(socketFD);

    printf("Closed socket\n");

    free_regex();
}

void handler(int v)
{
    printf("\n\nENDING\n");
    cleanup();
    exit(-2);
}

int main(int argc, char** argv)
{
    signal(SIGINT, handler);

    if (argc != 4) {
        printf("Incorrect Usage: web <addr> <port> <file>\n");
        return -1;
    }

    const char* addrLoc = argv[1];
    int port = atoi(argv[2]);
    sourceLoc = argv[3];

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

    setup_regex();

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

        parse_buf(clientSocket, readBuf, totalRead);

        close(clientSocket);
        printf("Closed socket\n");
    }

    cleanup();
}
