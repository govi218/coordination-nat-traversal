#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() != 1) {
        std::cerr << "Usage: ./peer [RECEIVER_IP]" << std::endl;
        return EXIT_FAILURE;
    }

    struct ifaddrs* ifaddr;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining head pointer so we
       can free list later. */

    for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        /* Display interface name and family (including symbolic
           form of the latter for the common families). */

        printf("%-8s %s (%d)\n", ifa->ifa_name,
               (family == AF_PACKET)  ? "AF_PACKET"
               : (family == AF_INET)  ? "AF_INET"
               : (family == AF_INET6) ? "AF_INET6"
                                      : "???",
               family);

        /* For an AF_INET* interface address, display the address. */

        if (family == AF_INET || family == AF_INET6) {
            s = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            printf("\t\taddress: <%s>\n", host);

        } else if (family == AF_PACKET && ifa->ifa_data != NULL) {
            struct rtnl_link_stats* stats =
                static_cast<rtnl_link_stats*>(ifa->ifa_data);

            printf(
                "\t\ttx_packets = %10u; rx_packets = %10u\n"
                "\t\ttx_bytes   = %10u; rx_bytes   = %10u\n",
                stats->tx_packets, stats->rx_packets, stats->tx_bytes,
                stats->rx_bytes);
        }
    }

    freeifaddrs(ifaddr);

    std::string priv_rcvr = args.at(0);  // who we send to
    std::vector<char> spMsg(100);
    spMsg.back() = 'e';

    int sock = -1;
    int ret;
    fd_set set;
    timeval tv = {};
    std::string strAddr;
    std::string strAddrLocal;

    // create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int enable = 1;
    if (sock < 0) {
        perror("Socket creation failed: ");
        return EXIT_FAILURE;
    }

    // set default receiver port (will be updated by whatever is detected from
    // received connection)
    int sender_port = 24573;
    int rcvr_port = 24573;

    // set source addr info
    struct sockaddr_in srcaddr;
    memset(&srcaddr, 0, sizeof(srcaddr));
    srcaddr.sin_family = AF_INET;
    srcaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srcaddr.sin_port = htons(sender_port);

    if (bind(sock, (struct sockaddr*)&srcaddr, sizeof(srcaddr)) < 0) {
        perror("bind");
        exit(1);
    }

    // establish UDP connection with private peer
    while (true) {
        // Setting up the sockaddr_in to connect to
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(rcvr_port);

        if (inet_pton(AF_INET, priv_rcvr.c_str(), &server.sin_addr) < 0) {
            perror("Invalid IP address");
            return EXIT_FAILURE;
        }

        ret = ::sendto(sock, spMsg.data(), spMsg.size(), 0,
                       (struct sockaddr*)&server, sizeof(server));

        if (ret < 0) {
            perror("UDP send failed: ");
            return EXIT_FAILURE;
        }

        // now wait for a response
        FD_ZERO(&set);
        FD_SET(sock, &set);
        tv.tv_usec = 250000;
        tv.tv_sec = 0;

        ret = select(sock + 1, &set, NULL, NULL, &tv);
        if (ret > 0) {
            socklen_t s_len = sizeof(server);
            ret = ::recvfrom(sock, spMsg.data(), spMsg.size(), 0,
                             (struct sockaddr*)&server, &s_len);

            if (ret > 0) {
                std::cout << "Got response (" << ret << " bytes) from "
                          << priv_rcvr << ":" << rcvr_port << std::endl;
                std::string udpMsg(spMsg.begin(), spMsg.end());
                std::cout << "Received UDP: " << udpMsg;

                // get peer details
                char host[NI_MAXHOST];
                char port[NI_MAXSERV];
                int error;
                int flags = NI_NUMERICHOST | NI_NUMERICSERV;

                error = getnameinfo((struct sockaddr*)&(server), sizeof(server),
                                    host, NI_MAXHOST, port, NI_MAXSERV, flags);

                // update receiver port
                rcvr_port = std::atoi(port);
            }
        } else {
            std::cout << "No connections received" << std::endl;
        }
    }
    return EXIT_SUCCESS;
}
