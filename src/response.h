#pragma once

#include <stdlib.h>

extern void setup(char* sourceLoc);
extern void cleanup();

void handle_msg(int fd, char* data, size_t length);
