#pragma once

#include <stdlib.h>

#include <netinet/in.h>
#include <sys/socket.h>

extern void setup(char *sourceLoc);
extern void cleanup();

void handle_msg(struct sockaddr *destAddr, socklen_t addrLen, int sendFD,
                char *data, size_t length);
