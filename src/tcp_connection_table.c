#include "tcp_connection_table.h"

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

#include "arpa/inet.h"
#include "netinet/in.h"

HashTable _Table;

void init_tcp_table()
{
    _Table.current_size = 16;
    _Table.element_count = 0;
    _Table.entries = calloc(_Table.current_size, sizeof(HashTableEntry));
}

uint32_t hash_port(HashTable* table, uint16_t port) { return port % table->current_size; }

HashTableEntry* new_entry(HashTable*, uint32_t, uint16_t);

void resize_table(HashTable* table)
{
    HashTable new_table = {
        .current_size = table->current_size * 2,
        .element_count = table->element_count,
    };

    new_table.entries = calloc(new_table.current_size, sizeof(HashTableEntry));

    for (size_t i = 0; i < table->current_size; i++) {
        HashTableEntry entry = table->entries[i];
        if (entry.current_status == Live) {
            HashTableEntry* inserted_entry
                = new_entry(&new_table, entry.stored_ip, entry.stored_port);

            inserted_entry->tcp_connection_status = entry.tcp_connection_status;
        }
    }
    *table = new_table;
}

HashTableEntry* new_entry(HashTable* table, uint32_t ip, uint16_t port)
{
    if (table->current_size == table->element_count) {
        resize_table(table);
    }

    HashTableEntry new_entry = { .current_status = Live,
        .stored_ip = ip,
        .stored_port = port,
        .tcp_connection_status = Active };

    table->element_count++;
    uint32_t hash = hash_port(table, port);
    while (1) {
        HashTableEntry entry = table->entries[hash];
        if (entry.current_status == Empty || entry.current_status == Tombstone) {
            table->entries[hash] = new_entry;
            return &table->entries[hash];
        } else {
            if (entry.stored_ip == ip && entry.stored_port == port) {
                table->entries[hash] = new_entry;
                return &table->entries[hash];
            }

            hash = (hash + 1) % table->current_size;
        }
    }
}

HashTableEntry* get_entry(HashTable* table, uint32_t ip, uint16_t port)
{
    uint32_t hash = hash_port(table, port);
    uint32_t initial_hash = hash;

    while (1) {
        HashTableEntry entry = table->entries[hash];
        if (entry.current_status == Empty) {
            return NULL;
        } else {
            if (entry.current_status == Live && entry.stored_port == port
                && entry.stored_ip == ip) {
                return &table->entries[hash];
            }

            hash = (hash + 1) % table->current_size;
            if (hash == initial_hash)
                return NULL;
        }
    }
    return NULL;
}

void remove_entry(HashTable* table, uint32_t ip, uint16_t port)
{
    HashTableEntry* entry = get_entry(table, ip, port);
    if (entry == NULL)
        return;

    table->element_count--;
    entry->current_status = Tombstone;
    entry->tcp_connection_status = Disconnected;
}

TCPConnection get_tcp_status(uint32_t ip, uint16_t port)
{
    HashTableEntry* entry = get_entry(&_Table, ip, port);
    if (entry == NULL) {
        return Invalid;
    }
    return entry->tcp_connection_status;
}

void start_connection(uint32_t ip, uint16_t port) { new_entry(&_Table, ip, port); }

void change_tcp_connection(uint32_t ip, uint16_t port, TCPConnection new_status)
{
    HashTableEntry* entry = get_entry(&_Table, ip, port);
    if (entry == NULL) {
        return;
    }

    entry->tcp_connection_status = new_status;
}

void end_connection(uint32_t ip, uint16_t port) { remove_entry(&_Table, ip, port); }

void print_connections()
{
    printf("----- TCP Connections -----\n");
    for (int i = 0; i < _Table.current_size; i++) {
        HashTableEntry entry = _Table.entries[i];
        if (entry.current_status == Live || entry.current_status == Tombstone) {
            printf("    %s:%u -> ", inet_ntoa(*(struct in_addr*)&entry.stored_ip),
                ntohs(entry.stored_port));
            switch (entry.tcp_connection_status) {
            case Active:
                printf("Active\n");
                break;
            case Disconnected:
                printf("Disconnected\n");
                break;
            case Finish:
                printf("Finished\n");
                break;
            default:
                printf("Unknown\n");
                break;
            }
        }
    }
    printf("----- TCP Connections -----\n");
}
