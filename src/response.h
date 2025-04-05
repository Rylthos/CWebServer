#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdlib.h>

#include <netinet/in.h>
#include <sys/socket.h>

extern void setup(char* sourceLoc);
extern void cleanup();

void handle_msg(struct sockaddr_in* srcAddr, struct sockaddr_in* destAddr, int seq_num, int ack_seq,
    int serverFD, uint8_t* buf, ssize_t buf_length);

#endif
