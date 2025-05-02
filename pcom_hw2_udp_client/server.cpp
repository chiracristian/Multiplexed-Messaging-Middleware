#include <iostream>
#include <vector>
#include <string.h>

#include <sys/socket.h>
#include <poll.h>

#include "utils.hpp"
#include <netinet/in.h>
#include <unistd.h>

#define STDIN_FD 0
#define MAX_STDIN_LEN 16

#define STDIN_POLLFD_IDX 0
#define UDP_POLLFD_IDX 1

int main(int argc, char** argv)
{
    int rc;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Check if only a parameter was passed
    DIE(argc != 2, "Usage: ./server <PORT>");

    // Extract the port
    unsigned read_port = std::atoi(argv[1]);
    DIE(read_port <= 0 || read_port > 65535,
        "The port must be a number between 1 and 65535");

    uint16_t port = read_port;

    // Open the UDP socket 
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket < 0, "Failed to open UDP socket");

    // Make it so datagrams can be received from any IP on the given port
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    rc = bind(udp_socket, (const sockaddr*)&server_address, sizeof(sockaddr_in));
    DIE(rc < 0, "Failed to bind UDP socket to port");

    // Setup polling
    std::vector<pollfd> poll_fds;

    pollfd stdin_pollfd;
    stdin_pollfd.fd = STDIN_FD;
    stdin_pollfd.events = POLLIN;
    poll_fds.push_back(stdin_pollfd);

    pollfd udp_pollfd;
    udp_pollfd.fd = udp_socket;
    udp_pollfd.events = POLLIN;
    poll_fds.push_back(udp_pollfd);

    while (true) {
        rc = poll(poll_fds.data(), poll_fds.size(), -1);
        DIE(rc < 0, "Failed to poll");

        // Check if we got input from stdin
        if (poll_fds[STDIN_POLLFD_IDX].revents & POLLIN) {
            char input_command[MAX_STDIN_LEN];
            if (fgets(input_command, MAX_STDIN_LEN, stdin) != NULL)
                if (strcmp(input_command, "exit\n") == 0)
                    break;
        }

        // Check if we got an UDP datagram
        if (poll_fds[UDP_POLLFD_IDX].revents & POLLIN) {
            char buffer[2048];
            sockaddr_in client_address;
            socklen_t sockaddr_in_size = sizeof(sockaddr_in);
            ssize_t bytes_recv = recvfrom(udp_socket, buffer, 2047, 0,
                                         (sockaddr *)&client_address, &sockaddr_in_size);

            if (bytes_recv > 0) {
                buffer[bytes_recv] = '\0';
                printf("Received UDP packet: %s\n", buffer);
            }
        }
    }

    close(udp_socket);

    return 0;
}