#include <arpa/inet.h>
#include <errno.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int socketFD;

const char* htmlHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
const char* pngHeaderString = "HTTP/1.1 200 OK\nContent-Type: images/png\n\n";
const char* jpgHeaderString = "HTTP/1.1 200 OK\nContent-Type: images/jpeg\n\n";
const char* cssHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/css\n\n";
const char* jsHeaderString = "HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n";

regex_t getRegex;

void setupRegex()
{
    int retV
        = regcomp(&getRegex, "GET \\([[:alnum:][:punct:]]*\\) HTTP/[[:digit:]].[[:digit:]]", 0);
    if (retV) {
        fprintf(stderr, "Failed to create regex\n");
    }
}

void freeRegex() { regfree(&getRegex); }

void get(int fd, char* data, size_t length)
{
    if (data[0] == '/') {
        FILE* file;

        const char** header;

        if (!strncmp(data, "/", length)) {
            file = fopen("resources/index.html", "r");

            if (file == NULL) {
                fprintf(stderr, "Failed to open file: resources/index.html\n");
                return;
            }

            header = &htmlHeaderString;
        } else {
            char buf[500];
            sprintf(buf, "resources/%.*s", (int)length - 1, data + 1);

            if (file == NULL) {
                fprintf(stderr, "Failed to open file: %s\n", buf);
                return;
            }

            switch (data[length - 1]) {
            case 'l': // HTML
                header = &htmlHeaderString;
                file = fopen(buf, "r");
                break;
            case 'g': // JPG, PNG
                switch (data[length - 2]) {
                case 'n': // PNG
                    header = &pngHeaderString;
                    break;
                case 'p': // JPG
                    header = &jpgHeaderString;
                    break;
                }
                file = fopen(buf, "rb");
                break;
            case 's': // CSS, JS
                switch (data[length - 2]) {
                case 's': // CSS
                    header = &cssHeaderString;
                    break;
                case 'j': // JS
                    header = &jsHeaderString;
                    break;
                }
                file = fopen(buf, "r");
                break;
            }
        }

        fseek(file, 0, SEEK_END);

        size_t length = ftell(file) + strlen(*header);
        fseek(file, 0, SEEK_SET);

        char* buf = malloc(length * sizeof(char) + 1);
        memcpy(buf, *header, strlen(*header));
        fread(buf + strlen(*header), 1, length - strlen(*header), file);
        buf[length] = 0;

        printf("Reply:\n===========\n%.*s==========\n", (int)length, buf);
        write(fd, buf, length);

        free(buf);
    }
}

void parse_buf(int fd, char* data, size_t length)
{
    printf("Received from %d:\n=====\n%.*s=====\n", fd, (int)length, data);

    regmatch_t* match = malloc((getRegex.re_nsub + 1) * sizeof(regmatch_t));
    int retV = regexec(&getRegex, data, getRegex.re_nsub + 1, match, 0);
    if (!retV) { // GET Request
        printf("%d->%d: %.*s\n", match[0].rm_so, match[0].rm_eo, match[0].rm_eo - match[0].rm_so,
            data + match[0].rm_so);

        printf("%d->%d: %.*s\n", match[1].rm_so, match[1].rm_eo, match[1].rm_eo - match[1].rm_so,
            data + match[1].rm_so);

        get(fd, data + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
    }

    free(match);
}

void cleanup()
{
    close(socketFD);

    printf("Closed socket\n");

    freeRegex();
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

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (!socketFD) {
        fprintf(stderr, "Failed to create socket\n");
        exit(-1);
    }
    printf("Started socket\n");

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = { .s_addr = inet_addr("127.0.0.1"), },
    };

    if (bind(socketFD, (struct sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "Failed to bind socket: %d\n", errno);
        exit(-1);
    };
    printf("Bound socket\n");

    setupRegex();

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

        size_t bufLength = 500;
        char* buf = malloc(bufLength + 1);
        buf[bufLength] = 0;
        ssize_t msgLength = 0;
        size_t totalRead = 0;

        do {
            if (msgLength == bufLength) {
                size_t newLength = bufLength * 2;
                char* newData = malloc(newLength + 1);
                memcpy(newData, buf, bufLength);
                bufLength = newLength;
                free(buf);
                buf = newData;
                buf[newLength] = 0;
            }

            msgLength
                = recv(clientSocket, buf + totalRead, (bufLength - totalRead) * sizeof(char), 0);
            totalRead += msgLength;
        } while (msgLength == bufLength);

        parse_buf(clientSocket, buf, totalRead);

        close(clientSocket);
        printf("Closed socket\n");
    }

    cleanup();
}
