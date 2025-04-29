#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "protocols.h"
#include "response.h"
#include "tcp_connection_table.h"

const char* info_str = "Incorrect Usage: web <addr> <port> <folder> [-l][-t]\n"
                       "\t-l: Enable indepth logging\n"
                       "\t-t: Enable logging of current tcp connections\n";

typedef struct cliArguments {
    // REQUIRED
    const char* ip;
    uint16_t port;
    char** resource_loc;

    // OPTIONAL
    int enable_log;
    int enable_tcp_log;
} CLIArguments;

int serverFD;

CLIArguments parseCLI(int argc, char** argv)
{
    CLIArguments arguments;
    memset(&arguments, 0, sizeof(arguments));

    if (argc < 4) {
        ERROR("%s", info_str);
        exit(-1);
    }

    arguments.ip = argv[1];
    arguments.port = atoi(argv[2]);
    arguments.resource_loc = &argv[3];

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            arguments.enable_log = 1;
        }
        if (strcmp(argv[i], "-t") == 0) {
            arguments.enable_tcp_log = 1;
        }
    }

    return arguments;
}

void cleanup_socket()
{
    close(serverFD);

    LOG_GENERAL("Closed Socket\n");

    cleanup();
}

void int_handler(int v)
{
    LOG_GENERAL("\n\nEnding\n");
    cleanup_socket();
    exit(-2);
}

void getSeqAckPortIP(
    uint8_t* buffer, uint32_t* seq_num, uint32_t* ack_seq, uint16_t* port, uint32_t* ip)
{
    *seq_num = ntohl(getTCPHeader(buffer)->seq_num);
    *ack_seq = ntohl(getTCPHeader(buffer)->ack_num);
    *port = getTCPHeader(buffer)->src_port;
    *ip = getIPHeader(buffer)->src_addr;
}

int main(int argc, char** argv)
{
    signal(SIGINT, int_handler);

    CLIArguments args = parseCLI(argc, argv);

    // TCP Header must be created. IP header created for us
    serverFD = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (serverFD == -1) {
        ERROR("Failed to create socket: %s | %d\n", strerror(errno), errno);
        return -1;
    }
    GENERAL("Created send socket: %d\n", serverFD);

    int one = 1;
    const int* val = &one;
    if (setsockopt(serverFD, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1) {
        ERROR("Setsockopt failed\n");
        close(serverFD);
        exit(-1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(args.port),
        .sin_addr.s_addr = inet_addr(args.ip),
    };

    addr.sin_port = htons(args.port);
    if (bind(serverFD, (struct sockaddr*)&addr, sizeof(addr))) {
        ERROR("Failed to bind socket %d | %s\n", serverFD, strerror(errno));

        close(serverFD);
        exit(-1);
    };
    GENERAL("Bound socket at %s:%d\n", args.ip, args.port);

    setup_response(*args.resource_loc);
    if (args.enable_log) {
        GENERAL("Enabled logging\n");
        enable_log = 1;
    }

    if (args.enable_tcp_log) {
        GENERAL("Enabled tcp status logging\n");
    }

    init_tcp_table();

    size_t readBufLength = 500;
    char* readBuf = malloc(readBufLength + 1);
    readBuf[readBufLength] = 0;

    struct sockaddr_in clientAddr;

    ssize_t packet_size;
    uint8_t buffer[65535];

    uint32_t seq_num, ack_seq, ip;
    uint16_t port_number;

    while (1) {
        packet_size = recvfrom(serverFD, buffer, 65535, 0, NULL, NULL);

        if (packet_size == -1) {
            ERROR("Failed to read packet\n");

            continue;
        }

        LOG_RECV({
            LOG_GENERAL("Received %ld bytes\n", packet_size);
            printIPPacket(buffer, packet_size);
        });

        getSeqAckPortIP(buffer, &seq_num, &ack_seq, &port_number, &ip);

        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = port_number;
        clientAddr.sin_addr.s_addr = ip;

        if (ip == addr.sin_addr.s_addr && port_number == addr.sin_port) {
            continue;
        }

        PacketType type = getTCPPacketType(buffer);

        switch (type) {
        case SYN: // Respond with SYN ACK
        {
            RECV_MSG("SYN", clientAddr, addr, seq_num, ack_seq, getTCPLength(buffer, packet_size));

            if (get_tcp_status(ip, port_number) == Active) {
                GENERAL("Port already in use\n");
                continue;
            }

            uint8_t* packet;
            uint32_t packet_size;

            int new_seq_num = seq_num + 1;
            createSynAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet, &packet_size);

            int sent = sendto(serverFD, packet, packet_size, 0, (struct sockaddr*)&clientAddr,
                sizeof(clientAddr));

            if (sent == -1) {
                LOG_ERROR("Failed to send bytes\n");
            } else {
                SENT_MSG("SYN ACK", addr, clientAddr, ack_seq, new_seq_num,
                    getTCPLength(packet, packet_size));
            }

            LOG_SEND({
                LOG_INFO("Sent %d bytes. SYN ACK\n", sent);
                printIPPacket(packet, packet_size);
            });
            free(packet);

            start_connection(ip, port_number);

            break;
        }
        case PSH: {
            RECV_MSG("PSH", clientAddr, addr, seq_num, ack_seq, getTCPLength(buffer, packet_size));

            if (get_tcp_status(ip, port_number) == Disconnected) {
                GENERAL("Port not connected\n");
                continue;
            }

            int new_seq_num = seq_num + 1;
            uint8_t* data;
            uint32_t data_size;
            getTCPDataPacket(buffer, packet_size, &data, &data_size);

            handle_msg(&addr, &clientAddr, ack_seq, new_seq_num, serverFD, data, data_size);

            change_tcp_connection(ip, port_number, Finish);

            break;
        }
        case FIN: {
            RECV_MSG("FIN", clientAddr, addr, seq_num, ack_seq, getTCPLength(buffer, packet_size));

            TCPConnection current_status = get_tcp_status(ip, port_number);
            if (current_status != Active && current_status != Finish) {
                GENERAL("Port not connected\n");
                continue;
            }

            uint8_t* packet;
            uint32_t packet_size;

            int new_seq_num = seq_num + 1;
            createFinAckPacket(&addr, &clientAddr, ack_seq, new_seq_num, &packet, &packet_size);

            int sent = sendto(serverFD, packet, packet_size, 0, (struct sockaddr*)&clientAddr,
                sizeof(clientAddr));

            if (sent == -1) {
                printf("Failed to send bytes\n");
            } else {
                SENT_MSG("FIN ACK", addr, clientAddr, ack_seq, new_seq_num,
                    getTCPLength(packet, packet_size));
            }

            LOG_SEND({
                LOG_INFO("Sent %d bytes. ACK\n", sent);
                printIPPacket(packet, packet_size);
            });

            free(packet);

            // change_tcp_connection(ip, port_number, Finish);
            end_connection(ip, port_number);
            GENERAL("Closed connection\n");

            break;
        }
        case ACK:
            RECV_MSG("ACK", clientAddr, addr, seq_num, ack_seq, getTCPLength(buffer, packet_size));

            break;
        default:
            LOG_ERROR("Unhandled type");
            break;
        }

        if (args.enable_tcp_log) {
            GENERAL("ENTRIES: %d/%d\n", _Table.element_count, _Table.current_size);
            print_connections();
        }
    }

    cleanup_socket();
}
