// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025–2026 Cristian-Ioan-George Chira

#include <cstring>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <unistd.h>

#include "TopicTrie.hpp"
#include "utils.hpp"

#define MAX_LISTENED_CLIENTS 16

#define STDIN_FD 0
#define MAX_STDIN_LEN 16

#define STDIN_POLLFD_IDX 0
#define UDP_POLLFD_IDX 1
#define LISTNER_POLLFD_IDX 2
#define SUBS_START_IDX 3

struct subscriber_data {
    bool connected;
    int sockfd;
    std::unordered_set<std::string> topics;
};

int connect_new_client(int listenfd,
                       std::vector<pollfd>& poll_fds,
                       std::unordered_map<std::string, subscriber_data>& subscribers,
                       std::unordered_map<int, std::string>& connected_subscribers) {
    char connecting_id[ID_LENGTH + 1];
    memset(connecting_id, 0, sizeof(connecting_id));

    sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(sockaddr_in);

    // Accept connection
    int new_socket = accept(listenfd, (sockaddr*)&cli_addr, &cli_addr_len);
    if (new_socket < 0) {
        std::cerr << errno << " Failed to accept connection\n";
        return -1;
    }
    disable_nagle(new_socket);

    // Receive the id of the client
    int rc = recvall(new_socket, connecting_id, ID_LENGTH);
    if (rc <= 0) {
        close(new_socket);
        return -1;
    }

    // If another client with the same id is connected, kick it out
    if (subscribers.count(connecting_id) > 0 && subscribers[connecting_id].connected) {
        std::cout << "Client " << connecting_id << " already connected.\n";
        close(new_socket);
        return -1;
    }

    // Mark the client as connected and map its socket to its name
    subscribers[connecting_id].connected = true;
    subscribers[connecting_id].sockfd = new_socket;
    connected_subscribers[new_socket] = connecting_id;

    // Add it for polling
    pollfd new_pollfd;
    new_pollfd.fd = new_socket;
    new_pollfd.events = POLLIN;
    poll_fds.push_back(new_pollfd);

    // Print a message announcing the client was connected
    std::cout << "New client " << connecting_id << " connected from ";
    std::cout << inet_ntoa(cli_addr.sin_addr) << ':';
    std::cout << ntohs(cli_addr.sin_port) << ".\n";

    return 0;
}

void process_sub_requests(subcribe_request* request,
                          std::string& client_name,
                          subscriber_data& subscriber,
                          TopicTrie& trie) {
    if (request->operation == OPERATION_SUBSCRIBE) {
        if (subscriber.topics.count(request->topic) > 0) return;

        subscriber.topics.insert(request->topic);
        trie.add_subscription(request->topic, client_name);

        // Send ACK
        char header = HEADER_SUBSCRIBE_ACK;
        sendall(subscriber.sockfd, &header, 1);
        request->operation = OPERATION_ACK_SUB;
        sendall(subscriber.sockfd, request, sizeof(subcribe_request));

    } else if (request->operation == OPERATION_UNSUBSCRIBE) {
        if (subscriber.topics.count(request->topic) == 0) return;

        subscriber.topics.erase(request->topic);
        trie.remove_subscription(request->topic, client_name);

        // Send ACK
        char header = HEADER_SUBSCRIBE_ACK;
        sendall(subscriber.sockfd, &header, 1);
        request->operation = OPERATION_ACK_UNSUB;
        sendall(subscriber.sockfd, request, sizeof(subcribe_request));
    }
}

void send_msg_to_all_subs(std::string& topic_str,
                          message_with_header& response,
                          std::unordered_map<std::string, subscriber_data>& subscribers,
                          TopicTrie& trie) {
    // Query the Trie to get ONLY the IDs of clients who match this topic
    std::unordered_set<std::string> matching_ids = trie.find_subscribers(topic_str);

    for (const std::string& sub_id : matching_ids) {
        subscriber_data& subscriber = subscribers[sub_id];

        // Only send if the client is currently connected
        if (subscriber.connected) {
            char header = HEADER_MESSAGE;
            if (sendall(subscriber.sockfd, &header, 1) < 0) continue;
            sendall(subscriber.sockfd, &response, sizeof(message_with_header));
        }
    }
}

int main(int argc, char** argv) {
    int rc;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != 2, "Usage: ./server <PORT>");

    unsigned read_port = std::atoi(argv[1]);
    DIE(read_port < 1024 || read_port > 65535, "Port must be between 1024 and 65535");
    const uint16_t port = read_port;

    // Open UDP socket
    const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpfd < 0, "Failed to open UDP socket");

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    rc = bind(udpfd, (const sockaddr*)&server_address, sizeof(sockaddr_in));
    DIE(rc < 0, "Failed to bind UDP socket");

    // Open TCP listner socket
    const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "Failed to open TCP listner socket");
    disable_nagle(listenfd);

    rc = bind(listenfd, (const sockaddr*)&server_address, sizeof(sockaddr_in));
    DIE(rc < 0, "Failed to bind TCP socket");

    rc = listen(listenfd, MAX_LISTENED_CLIENTS);
    DIE(rc < 0, "Failed to listen");

    // Setup polling
    std::vector<pollfd> poll_fds = {
        {STDIN_FD, POLLIN, 0},
        {udpfd, POLLIN, 0},
        {listenfd, POLLIN, 0}
    };

    TopicTrie trie;
    std::unordered_map<std::string, subscriber_data> subscribers;
    std::unordered_map<int, std::string> connected_subscribers;

    while (true) {
        rc = poll(poll_fds.data(), poll_fds.size(), -1);
        DIE(rc < 0, "Failed to poll");

        // Handle STDIN
        if (poll_fds[STDIN_POLLFD_IDX].revents & POLLIN) {
            char input_command[MAX_STDIN_LEN];
            if (fgets(input_command, MAX_STDIN_LEN, stdin) != NULL)
                if (strcmp(input_command, "exit\n") == 0)
                    break;
        }

        // Handle UDP (Draining the socket for high-speed bursts)
        if (poll_fds[UDP_POLLFD_IDX].revents & POLLIN) {
            while (true) {
                sockaddr_in client_addr;
                socklen_t client_addr_size = sizeof(sockaddr_in);
                message received_message;

                rc = recvfrom(udpfd, &received_message, sizeof(message), 
                              MSG_DONTWAIT, (sockaddr*)&client_addr, &client_addr_size);
                if (rc < 0)
                    break;

                std::string topic_str(received_message.topic, strnlen(received_message.topic, MAX_TOPIC_LENGTH));
                message_with_header response = {client_addr.sin_addr.s_addr, client_addr.sin_port, received_message};
                
                send_msg_to_all_subs(topic_str, response, subscribers, trie);
            }
        }

        // Handle New TCP Connections
        if (poll_fds[LISTNER_POLLFD_IDX].revents & POLLIN) {
            connect_new_client(listenfd, poll_fds, subscribers, connected_subscribers);
        }

        // Iterate through TCP clients
        for (size_t i = SUBS_START_IDX; i < poll_fds.size(); i++) {
            if (poll_fds[i].revents & POLLIN) {
                int clientfd = poll_fds[i].fd;
                std::string& client_name = connected_subscribers[clientfd];
                subscriber_data& subscriber = subscribers[client_name];

                // Drain TCP socket to handle multiple commands sent in one segment
                while (true) {
                    subcribe_request sub_request;
                    rc = recv(clientfd, &sub_request, sizeof(subcribe_request), MSG_DONTWAIT);

                    if (rc == sizeof(subcribe_request)) {
                        process_sub_requests(&sub_request, client_name, subscriber, trie);
                    } else if (rc == 0 || (rc < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        // Handle disconnect (Keep subscriptions in trie for persistence)
                        std::cout << "Client " << client_name << " disconnected.\n";
                        subscriber.connected = false;
                        close(clientfd);

                        connected_subscribers.erase(clientfd);
                        poll_fds.erase(poll_fds.begin() + i--);
                        break;
                    } else 
                        break;
                }
            }
        }
    }

    // Global cleanup
    close(listenfd);
    close(udpfd);
    for (size_t i = SUBS_START_IDX; i < poll_fds.size(); i++)
        close(poll_fds[i].fd);

    return 0;
}
