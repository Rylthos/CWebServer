#pragma once

#include "stdint.h"

typedef enum _HashTableStatus { Empty, Tombstone, Live } HashTableStatus;
typedef enum _TCPConnection { Disconnected, Active, Invalid, Finish } TCPConnection;

typedef struct _HashTableEntry {
    HashTableStatus current_status;
    uint32_t stored_ip;
    uint16_t stored_port;
    TCPConnection tcp_connection_status;
} HashTableEntry;

typedef struct _HashTable {
    uint32_t current_size;
    uint32_t element_count;
    HashTableEntry* entries;
} HashTable;

extern HashTable _Table;

void init_tcp_table();

TCPConnection get_tcp_status(uint32_t ip, uint16_t port);
void start_connection(uint32_t ip, uint16_t port);
void change_tcp_connection(uint32_t ip, uint16_t port, TCPConnection new_connection);
void end_connection(uint32_t ip, uint16_t port);

void print_connections();
