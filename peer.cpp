#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
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
    perror("setsockopt(SO_REUSEADDR) failed");

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

    while (true) {
        // send UDP req to private peer
        std::string priv_rcvr = "135.84.106.98";  // who we send to
        // std::string priv_rcvr = "98.204.46.99";   // who we send to

        // Setting up the sockaddr_in to connect to
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(rcvr_port);

        if (inet_pton(AF_INET, priv_rcvr.c_str(), &server.sin_addr) < 0) {
            perror("client: inet_pton");
            return 1;
        }

        ret = ::sendto(sock, spMsg.data(), spMsg.size(), 0,
                       (struct sockaddr*)&server, sizeof(server));

        if (ret < 0)
            perror("error ");

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
                std::cout << "Received UDP: %s" << udpMsg;
                // clientlogic.ProcessResponse(spMsg, addrRemote, addrLocal);

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
    return 0;
}
