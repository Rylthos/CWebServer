#include "log.h"

#include "stdio.h"
#include "string.h"

#define MAX_STORE_LENGTH 16

void print_hex(uint8_t *buf, size_t length, const char *prefix) {
  static char store[MAX_STORE_LENGTH];

  static char fmt_string[80];
  strcpy(fmt_string, "\t%.*s\n");
  strcat(fmt_string, prefix);

  for (int i = 0; i < length; i++) {
    if (i != 0 && i % MAX_STORE_LENGTH == 0) {
      printf(fmt_string, MAX_STORE_LENGTH, store);
    }

    uint8_t data = *(buf + i);
    store[i % MAX_STORE_LENGTH] = data;
    if (data < 32)
      store[i % MAX_STORE_LENGTH] = '.';

    printf("%02x ", data);
  }
  printf("\t%.*s\n", (int)length % MAX_STORE_LENGTH, store);
}
