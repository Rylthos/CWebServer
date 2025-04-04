#pragma once

#include <stdint.h>
#include <stdlib.h>

#define F_LOG_GENERAL(file, ...) fprintf(file, __VA_ARGS__)

#define F_LOG_RECV_HEADER(file)                                                \
  fprintf(file, "******************** RECV ********************\n");

#define F_LOG_SEND_HEADER(file)                                                \
  fprintf(file, "******************** SEND ********************\n");

#define F_LOG_RECV(file, ...)                                                  \
  do {                                                                         \
    F_LOG_RECV_HEADER(file)                                                    \
    fprintf(file, __VA_ARGS__);                                                \
    F_LOG_RECV_HEADER(file)                                                    \
  } while (0)

#define F_LOG_SEND(file, ...)                                                  \
  do {                                                                         \
    F_LOG_SEND_HEADER(file)                                                    \
    fprintf(file, __VA_ARGS__);                                                \
    F_LOG_SEND_HEADER(file)                                                    \
  } while (0)

#define LOG_RECV_HEADER_START F_LOG_RECV_HEADER(stdout)
#define LOG_SEND_HEADER_START F_LOG_SEND_HEADER(stdout)

#define LOG_RECV_HEADER_END                                                    \
  do {                                                                         \
    F_LOG_RECV_HEADER(stdout);                                                 \
    F_LOG_GENERAL(stdout, "\n");                                               \
  } while (0)
#define LOG_SEND_HEADER_END                                                    \
  do {                                                                         \
    F_LOG_SEND_HEADER(stdout);                                                 \
    F_LOG_GENERAL(stdout, "\n");                                               \
  } while (0)

#define LOG_GENERAL(...) F_LOG_GENERAL(stdout, __VA_ARGS__)
#define LOG_RECV(...) F_LOG_RECV(stdout, __VA_ARGS__)
#define LOG_SEND(...) F_LOG_SEND(stdout, __VA_ARGS__)

void print_hex(const uint8_t *data, size_t length, const char *prefix);
