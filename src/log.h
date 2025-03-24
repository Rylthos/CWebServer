#pragma once

#define F_LOG_GENERAL(file, ...) fprintf(file, __VA_ARGS__)

#define F_LOG_RECV(file, ...)                                                  \
  do {                                                                         \
    fprintf(file, "******************** RECV ********************\n");         \
    fprintf(file, __VA_ARGS__);                                                \
    fprintf(file, "******************** RECV ********************\n");         \
  } while (0)

#define F_LOG_SEND(file, ...)                                                  \
  do {                                                                         \
    fprintf(file, "******************** SEND ********************\n");         \
    fprintf(file, __VA_ARGS__);                                                \
    fprintf(file, "******************** SEND ********************\n");         \
  } while (0)

#define LOG_GENERAL(...) F_LOG_GENERAL(stdout, __VA_ARGS__)
#define LOG_RECV(...) F_LOG_RECV(stdout, __VA_ARGS__)
#define LOG_SEND(...) F_LOG_SEND(stdout, __VA_ARGS__)
