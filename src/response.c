#include "response.h"
#include "log.h"
#include "protocols.h"

#include <errno.h>
#include <netinet/in.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

// Response Headers
static const char *errorString = "HTTP/1.1 404 Not Found\n";
static const char *htmlHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
static const char *pngHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: images/png\n\n";
static const char *jpgHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: images/jpeg\n\n";
static const char *svgHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: image/svg+xml\n\n";
static const char *cssHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: text/css\n\n";
static const char *jsHeaderString =
    "HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n";

static regex_t s_GetRegex;
static char *s_SourceLoc;

void setup(char *sourceLoc) {
  int retV = regcomp(
      &s_GetRegex,
      "GET \\([[:alnum:][:punct:]]*\\) HTTP/[[:digit:]].[[:digit:]]", 0);

  if (retV) {
    fprintf(stderr, "Failed to create regex\n");
  }

  s_SourceLoc = sourceLoc;
}

void cleanup() { regfree(&s_GetRegex); }

int parse_file_ext(char *filename, size_t length, char **fileBuf,
                   const char ***header) {
  if (!strncmp(filename, "/", length)) {
    sprintf(*fileBuf, "%s/index.html", s_SourceLoc);
    *header = &htmlHeaderString;
  } else {
    sprintf(*fileBuf, "%s/%.*s", s_SourceLoc, (int)length - 1,
            filename + 1); // Remove preceeding '/'
    switch (filename[length - 1]) {
    case 'l': // HTML
      *header = &htmlHeaderString;
      break;
    case 'g': // JPG, PNG, SVG
    {
      switch (filename[length - 2]) {
      case 'n': // PNG
        *header = &pngHeaderString;
        break;
      case 'p': // JPG
        *header = &jpgHeaderString;
        break;
      case 'v': // SVG
        *header = &svgHeaderString;
        break;
      }
      break;
    }
    case 's': // SVG, CSS, JS, TS
    {
      switch (filename[length - 2]) {
      case 's': // CSS
        *header = &cssHeaderString;
        break;
      case 'j': // JS
      case 't': // TS
        *header = &jsHeaderString;
        break;
      }
      break;
    }
    default:
      F_LOG_GENERAL(stderr, "Unknown file type: %.*s\n", (int)length, filename);
      *header = &errorString;
      return -1;
    }
  }

  return 0;
}

void send_msg(struct sockaddr_in *srcAddr, struct sockaddr_in *destAddr,
              int seq_num, int ack_seq, int serverFD, uint8_t *buf,
              ssize_t buf_length) {
  LOG_SEND("%.*s", (int)buf_length, buf);

  int max_buf_size = MAX_DATAGRAM_SIZE - sizeof(TCPHeader) - sizeof(IPHeader);

  for (int i = 0; i < buf_length; i += max_buf_size) {
    uint8_t *current_buf_pos = buf + i;
    uint32_t buf_size = buf_length - i;
    if (buf_size > max_buf_size) {
      buf_size = max_buf_size;
    }

    uint32_t packet_size;
    uint8_t *packet;
    createDataPacket(srcAddr, destAddr, seq_num + i, ack_seq, current_buf_pos,
                     buf_size, &packet, &packet_size);

    int sent = sendto(serverFD, packet, packet_size, 0,
                      (struct sockaddr *)destAddr, sizeof(*destAddr));

    if (sent == -1) {
      fprintf(stderr, "Failed to send msg: %d\n", errno);
    } else {
      printf("Sent %d bytes\n", sent);
    }

    LOG_SEND_HEADER_START;
    printIPPacket(packet, packet_size);
    printTCPSegment(packet + sizeof(IPHeader), packet_size - sizeof(IPHeader));
    LOG_SEND_HEADER_END;

    free(packet);
  }
}

void send_file(struct sockaddr_in *srcAddr, struct sockaddr_in *destAddr,
               int seq_num, int ack_seq, int serverFD, char *filename,
               ssize_t length) {
  FILE *file;

  const char **header;

  char *fileBuf = malloc(500 * sizeof(char));
  if (parse_file_ext(filename, length, &fileBuf, &header)) { // Error occured
    send_msg(srcAddr, destAddr, seq_num, ack_seq, serverFD, (uint8_t *)fileBuf,
             strlen(fileBuf));
    return;
  }

  file = fopen(fileBuf, "r");

  if (file == NULL) {
    LOG_GENERAL("File doesnt exist: %.*s\n", (int)length, filename);
    send_msg(srcAddr, destAddr, seq_num, ack_seq, serverFD,
             (uint8_t *)errorString, strlen(errorString));
    return;
  }

  fseek(file, 0, SEEK_END);

  size_t totalLength = ftell(file) + strlen(*header);
  fseek(file, 0, SEEK_SET);

  char *buf = malloc(totalLength * sizeof(char) + 1);
  memcpy(buf, *header, strlen(*header));
  fread(buf + strlen(*header), 1, totalLength - strlen(*header), file);
  buf[totalLength] = 0;

  send_msg(srcAddr, destAddr, seq_num, ack_seq, serverFD, (uint8_t *)buf,
           totalLength);

  free(buf);
  free(fileBuf);
}

void handle_get(struct sockaddr_in *srcAddr, struct sockaddr_in *destAddr,
                int seq_num, int ack_seq, int serverFD, uint8_t *buf,
                ssize_t buf_length) {
  if (buf[0] == '/') {
    send_file(srcAddr, destAddr, seq_num, ack_seq, serverFD, (char *)buf,
              buf_length);
  }
}

void handle_msg(struct sockaddr_in *srcAddr, struct sockaddr_in *destAddr,
                int seq_num, int ack_seq, int serverFD, uint8_t *buf,
                ssize_t buf_length) {
  regmatch_t *match = malloc((s_GetRegex.re_nsub + 1) * sizeof(regmatch_t));
  int retV =
      regexec(&s_GetRegex, (char *)buf, s_GetRegex.re_nsub + 1, match, 0);

  if (!retV) { // handle get Request
    handle_get(srcAddr, destAddr, seq_num, ack_seq, serverFD,
               buf + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
  } else {
    printf("Unknown request:\n%.*s\n", (int)buf_length, buf);
  }

  free(match);
}
