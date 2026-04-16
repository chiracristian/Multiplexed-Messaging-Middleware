// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025–2026 Cristian-Ioan-George Chira

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "utils.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#define STDIN_FD 0
#define MAX_STDIN_LEN 128
#define STDIN_POLLFD_IDX 0
#define SOCKFD_IDX 1

#define SUBSCRIBE_COMMAND_ENTERED 0
#define EXIT_COMMAND_ENTERED 1
#define INVALID_COMMAND_ENTERED 2

int process_input_comand(char* input_command, int sockfd)
{
    // Get the first word of the command
    char* current_token = strtok(input_command, " \n");

    // If it's "exit", return so
    if (strcmp(current_token, "exit") == 0)
        return EXIT_COMMAND_ENTERED;

    // Else it must be "subscribe"/"unsubscribe"
    uint8_t operation;
    if (strcmp(current_token, "subscribe") == 0)
        operation = OPERATION_SUBSCRIBE;
    else if (strcmp(current_token, "unsubscribe") == 0)
        operation = OPERATION_UNSUBSCRIBE;
    else
        return INVALID_COMMAND_ENTERED;

    // Get the second word of the command (the topic)
    current_token = strtok(NULL, " \n");
    if (current_token == NULL)
        return INVALID_COMMAND_ENTERED;

    // Prepare the (un)subscribe request
    subcribe_request request;
    request.operation = operation;
    strncpy(request.topic, current_token, MAX_TOPIC_LENGTH);
    request.topic[MAX_TOPIC_LENGTH] = '\0';

    // Send the request to the server
    int rc = sendall(sockfd, (const char*)&request, sizeof(subcribe_request));
    if (rc < 0)
        std::cerr << "Failed to send request\n";

    return SUBSCRIBE_COMMAND_ENTERED;
}

int receive_message(int sockfd)
{
    message_with_header pkt;

    // Receive the message
    int rc = recvall(sockfd, &pkt, sizeof(message_with_header));
    if (rc <= 0) {
        std::cerr << "Failed to receive message\n";
        return -1;
    }

    // Print IP and port of sender
    in_addr source_ip;
    source_ip.s_addr = pkt.source_ip_address;
    std::cout << inet_ntoa(source_ip) << ':' << ntohs(pkt.source_port);

    // Handle the case the topic has exactly MAX_TOPIC_LENGTH characters
    std::string topic(pkt.msg.topic, strnlen(pkt.msg.topic, MAX_TOPIC_LENGTH));

    // Print topic and type
    std::cout << " - " << topic << " - " << pkt.msg.get_data_type();

    // Print content
    std::cout << " - " << pkt.msg.get_displayed_content() << '\n';

    // Success
    return 0;
}

int receive_subscribe_ack(int sockfd)
{
    subcribe_request request;

    // Receive the acknowledgement
    int rc = recvall(sockfd, &request, sizeof(subcribe_request));
    if (rc <= 0) {
        std::cerr << "Failed to receive subscription acknoledgement\n";
        return -1;
    }

    // Display a corresponding message
    if (request.operation == OPERATION_ACK_SUB)
        std::cout << "Subscribed to topic " << request.topic << '\n';
    else if (request.operation == OPERATION_ACK_UNSUB)
        std::cout << "Unsubscribed from topic " << request.topic << '\n';
    else
        std::cerr << "Invalid acknoledgement";

    // Success
    return 0;
}

int main(int argc, char **argv) {
    int rc;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Check if 3 parameters were passed
    DIE(argc != 4, "Usage: ./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>");

    // Read the client ID
    char client_id[ID_LENGTH+1];
    memset(client_id, 0, ID_LENGTH);
    strncpy(client_id, argv[1], ID_LENGTH);

    // Read the server IP
    uint32_t server_ip;
    rc = inet_pton(AF_INET, argv[2], &server_ip);
    DIE(rc <= 0, "Incorrect IP address");

    // Read the corresponding port
    unsigned read_port = std::atoi(argv[3]);
    DIE(read_port < 1024 || read_port > 65535,
        "The port must be a number between 1024 and 65535");
    const uint16_t port = read_port;

    // Open a TCP socket
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Failed to open the TCP socket");
    disable_nagle(sockfd);

    // Complete the server address fields
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_ip;
    server_address.sin_port = htons(port);

    // Connect to the server
    rc = connect(sockfd, (struct sockaddr *)&server_address, sizeof(sockaddr_in));
    DIE(rc < 0, "Failed to connect to the server");

    // Send ID to server
    rc = sendall(sockfd, client_id, ID_LENGTH);
    DIE(rc <= 0, "Failed to send the id to the server");

    // Setup polling
    std::vector<pollfd> poll_fds;

    pollfd stdin_pollfd;
    stdin_pollfd.fd = STDIN_FD;
    stdin_pollfd.events = POLLIN;
    poll_fds.push_back(stdin_pollfd);

    pollfd socket_pollfd;
    socket_pollfd.fd = sockfd;
    socket_pollfd.events = POLLIN;
    poll_fds.push_back(socket_pollfd);

    char input_command[MAX_STDIN_LEN];

    while (true) {
        rc = poll(poll_fds.data(), poll_fds.size(), -1);
        DIE(rc < 0, "Failed to poll");

        // Check if we got input from stdin
        if (poll_fds[STDIN_POLLFD_IDX].revents & POLLIN)
            if (fgets(input_command, MAX_STDIN_LEN, stdin) != NULL) {
                rc = process_input_comand(input_command, sockfd);
                if (rc == EXIT_COMMAND_ENTERED)
                    break;
            }

        // Check if we received a TCP packet
        if (poll_fds[SOCKFD_IDX].revents & POLLIN) {
            // Receive at most one byte, as header
            uint8_t recv_header;
            rc = recvall(sockfd, &recv_header, sizeof(uint8_t));
            
            if (rc <= 0) {
                // Exit if server disconnected
                break;
            } else {
                // Depending on the byte, we either got a message or
                // an (un)subscribe acknowledgement
                if (recv_header == HEADER_MESSAGE)
                    rc = receive_message(sockfd);
                else if (recv_header == HEADER_SUBSCRIBE_ACK)
                    rc = receive_subscribe_ack(sockfd);
                else
                    std::cerr << "Invalid header of packet detected\n";

                // Check if the server disconnected while receiving data
                if (rc < 0)
                    break;
            }
        }
    }

    // Close the TCP socket at the end
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
}
