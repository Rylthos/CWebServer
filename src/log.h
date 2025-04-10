#pragma once

#include <stdint.h>
#include <stdlib.h>

extern int enable_log;

#define F_LOG_GENERAL(file, ...) fprintf(file, __VA_ARGS__)

#define F_LOG_RECV_HEADER(file) fprintf(file, "******************** RECV ********************\n");
#define F_LOG_SEND_HEADER(file) fprintf(file, "******************** SEND ********************\n");

#define F_LOG_RECV(file, ...)                                                                      \
    do {                                                                                           \
        F_LOG_RECV_HEADER(file)                                                                    \
        fprintf(file, __VA_ARGS__);                                                                \
        F_LOG_RECV_HEADER(file)                                                                    \
    } while (0)

#define F_LOG_SEND(file, ...)                                                                      \
    do {                                                                                           \
        F_LOG_SEND_HEADER(file)                                                                    \
        fprintf(file, __VA_ARGS__);                                                                \
        F_LOG_SEND_HEADER(file)                                                                    \
    } while (0)

#define LOG_RECV_HEADER_START F_LOG_RECV_HEADER(stdout)
#define LOG_SEND_HEADER_START F_LOG_SEND_HEADER(stdout)

#define LOG_RECV_HEADER_END                                                                        \
    do {                                                                                           \
        F_LOG_RECV_HEADER(stdout);                                                                 \
        F_LOG_GENERAL(stdout, "\n");                                                               \
    } while (0)

#define LOG_SEND_HEADER_END                                                                        \
    do {                                                                                           \
        F_LOG_SEND_HEADER(stdout);                                                                 \
        F_LOG_GENERAL(stdout, "\n");                                                               \
    } while (0)

#define ERROR(...) F_LOG_GENERAL(stderr, __VA_ARGS__)
#define GENERAL(...) F_LOG_GENERAL(stdout, __VA_ARGS__)

#define MSG(DIR, PACKET_TYPE, SRC, DST, SEQ, ACK, LEN)                                             \
    GENERAL("%s: %s | %s:%5u -> %s:%5u | SEQ NUM: %u | ACK SEQ: %u | LENGTH: %u\n", (DIR),         \
        (PACKET_TYPE), inet_ntoa((SRC).sin_addr), ntohs((SRC).sin_port),                           \
        inet_ntoa((DST).sin_addr), ntohs((DST).sin_port), (SEQ), (ACK), (LEN))

#define SENT_MSG(PACKET_TYPE, SRC, DST, SEQ, ACK, LEN)                                             \
    MSG("SENT", PACKET_TYPE, SRC, DST, SEQ, ACK, LEN)

#define RECV_MSG(PACKET_TYPE, SRC, DST, SEQ, ACK, LEN)                                             \
    MSG("RECV", PACKET_TYPE, SRC, DST, SEQ, ACK, LEN)

#define CHECK_LOG(BLOCK)                                                                           \
    do {                                                                                           \
        if (enable_log) {                                                                          \
            BLOCK                                                                                  \
        }                                                                                          \
    } while (0)

#define LOG_RECV(block)                                                                            \
    CHECK_LOG({                                                                                    \
        LOG_RECV_HEADER_START;                                                                     \
        block;                                                                                     \
        LOG_RECV_HEADER_END;                                                                       \
    })

#define LOG_SEND(block)                                                                            \
    CHECK_LOG({                                                                                    \
        LOG_SEND_HEADER_START;                                                                     \
        block;                                                                                     \
        LOG_SEND_HEADER_END;                                                                       \
    })

#define LOG_GENERAL(...) CHECK_LOG({ F_LOG_GENERAL(stdout, "[LOG]: " __VA_ARGS__); })

#define LOG_ERROR(...) CHECK_LOG({ F_LOG_GENERAL(stderr, "[ERROR]: " __VA_ARGS__); })
#define LOG_INFO(...) CHECK_LOG({ F_LOG_GENERAL(stderr, __VA_ARGS__); })

void print_hex(const uint8_t* data, size_t length, const char* prefix);
