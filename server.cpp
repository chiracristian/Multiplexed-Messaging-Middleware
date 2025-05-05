#include <cstring>
#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#include "utils.hpp"
#include <netinet/in.h>
#include <unistd.h>

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
    std::vector<std::string> topics;
};

int connect_new_client(int listenfd,
                       std::vector<pollfd>& poll_fds,
                       std::unordered_map<std::string, subscriber_data>& subscribers,
                       std::unordered_map<int, std::string>& connected_subscribers) {
    char connecting_id[ID_LENGTH+1];
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
    if (rc < 0) {
        shutdown(new_socket, SHUT_RDWR);
        close(new_socket);
        return -1;
    }
    connecting_id[rc] = '\0';

    // If another client with the same id is connected, kick it out
    if (subscribers.count(connecting_id) > 0 && subscribers[connecting_id].connected) {
        std::cout << "Client " << connecting_id << " already connected.\n";
        shutdown(new_socket, SHUT_RDWR);
        close(new_socket);
        return -1;
    }

    // Mark the client as connected in the hash table
    subscribers[connecting_id].connected = true;
    subscribers[connecting_id].sockfd = new_socket;

    // Add it into the map of sockets and connected client IDs
    connected_subscribers[new_socket] = connecting_id;

    // Add it for polling
    pollfd new_pollfd;
    new_pollfd.fd = new_socket;
    new_pollfd.events = POLLIN;
    poll_fds.push_back(new_pollfd);

    // Print a message announcing the client was connected
    std::cout << "New client " << connecting_id << " connected from ";
    std::cout << inet_ntoa(cli_addr.sin_addr) << ':';
    std::cout << htons(cli_addr.sin_port) << ".\n";

    // Success
    return 0;
}

bool matches_topic(const char* received_topic, const char* subscribed_topic)
{
    return (strcmp(received_topic, subscribed_topic) == 0);
}

void process_subscription_requests(char *received_data,
                           std::string& client_name,
                           subscriber_data& subscriber,
                           int clientfd)
{
    subcribe_request* request = (subcribe_request*)received_data;

    if (request->operation == OPERATION_SUBSCRIBE) {
        bool topic_already_added = false;

        // Check if the client is not already subscribed to that topic
        for (size_t i = 0; i < subscriber.topics.size(); i++) {
            if (strcmp(request->topic, subscriber.topics[i].c_str()) == 0) {
                topic_already_added = true;
                break;
            }
        }

        if (topic_already_added)
            return;

        // Add the topic to client's subscription list
        subscriber.topics.push_back(request->topic);

        // Send a byte to prepare the client for receiving an acknowledgement
        // for subscribing
        char header = HEADER_SUBSCRIBE_ACK;
        int rc = sendall(subscriber.sockfd, &header, 1);
        if (rc < 0)
            std::cerr << "Failed to send ack header to " << client_name << '\n';

        // Send back the acknowledgement of subscribing
        request->operation = OPERATION_ACK_SUB;
        rc = sendall(clientfd, request, sizeof(subcribe_request));
        if (rc < 0) {
            std::cerr << "Failed to send subscribe ACK\n";
        }
    } else if (request->operation == OPERATION_UNSUBSCRIBE) {
        bool topic_existed = false;

        // Check if the client is currently subscribed to what it wants to unsubscribe from 
        for (size_t i = 0; i < subscriber.topics.size(); i++) {
            if (strcmp(request->topic, subscriber.topics[i].c_str()) == 0) {
                topic_existed = true;

                // Remove the topic from client's subscription list
                subscriber.topics.erase(subscriber.topics.begin() + i);
                break;
            }
        }
        if (!topic_existed)
            return;

        // Send a byte to prepare the client for receiving an acknowledgement
        // for unsubscribing
        char header = HEADER_SUBSCRIBE_ACK;
        int rc = sendall(subscriber.sockfd, &header, 1);
        if (rc < 0)
            std::cerr << "Failed to send ack header to " << client_name << '\n';

        // Send back the acknowledgement of unsubscribing
        request->operation = OPERATION_ACK_UNSUB;
        rc = sendall(clientfd, request, sizeof(subcribe_request));
        if (rc < 0)
            std::cerr << "Failed to send unsubscribe ACK\n";
    } else {
        std::cerr << "Invalid request from a TCP client\n";
    }
}

int main(int argc, char** argv)
{
    int rc;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Check if only a parameter was passed
    DIE(argc != 2, "Usage: ./server <PORT>");

    // Extract the port
    unsigned read_port = std::atoi(argv[1]);
    DIE(read_port < 1024 || read_port > 65535,
        "The port must be a number between 1024 and 65535");
    const uint16_t port = read_port;

    // Open the UDP socket 
    const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpfd < 0, "Failed to open UDP socket");

    // Complete the server address fields
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // Bind the UDP socket to the port
    rc = bind(udpfd, (const sockaddr*)&server_address, sizeof(sockaddr_in));
    DIE(rc < 0, "Failed to bind the UDP socket to the port");

    // Create a TCP listner socket
    const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "Failed to open TCP listner socket\n");
    disable_nagle(listenfd);

    // Bind the TCP listner socket to the port
    rc = bind(listenfd, (const sockaddr*)&server_address, sizeof(sockaddr_in));

    // Make the TCP listner socket listen
    rc = listen(listenfd, MAX_LISTENED_CLIENTS);
    DIE(rc < 0, "Failed to enable the listner TCP socket for listening\n");

    // Setup polling
    std::vector<pollfd> poll_fds;

    pollfd stdin_pollfd;
    stdin_pollfd.fd = STDIN_FD;
    stdin_pollfd.events = POLLIN;
    poll_fds.push_back(stdin_pollfd);

    pollfd udp_pollfd;
    udp_pollfd.fd = udpfd;
    udp_pollfd.events = POLLIN;
    poll_fds.push_back(udp_pollfd);

    pollfd listner_pollfd;
    listner_pollfd.fd = listenfd;
    listner_pollfd.events = POLLIN;
    poll_fds.push_back(listner_pollfd);

    std::unordered_map<std::string, subscriber_data> subscribers;
    std::unordered_map<int, std::string> connected_subscribers;

    std::queue<std::pair<std::string, message_with_header>> waiting_messages;

    char input_command[MAX_STDIN_LEN];
    char received_datagram[sizeof(message)];
    char received_from_TCP[sizeof(subcribe_request)];
    while (true) {
        rc = poll(poll_fds.data(), poll_fds.size(), -1);
        DIE(rc < 0, "Failed to poll");

        // Check if we got input from stdin
        if (poll_fds[STDIN_POLLFD_IDX].revents & POLLIN)
            if (fgets(input_command, MAX_STDIN_LEN, stdin) != NULL)
                if (strcmp(input_command, "exit\n") == 0)
                    break;

        // Check if we got an UDP datagram
        if (poll_fds[UDP_POLLFD_IDX].revents & POLLIN) {
            sockaddr_in client_addr;
            socklen_t client_addr_size = sizeof(sockaddr_in);
            rc = recvfrom(udpfd, received_datagram, sizeof(received_datagram),
                          0, (sockaddr*)&client_addr, &client_addr_size);
            if (rc < 0) {
                std::cerr << "Failed to receive UDP datagram\n";
                continue;
            }
            message* received_message = (message*)received_datagram;
            std::pair<std::string, message_with_header> topicMessagePair;
            topicMessagePair.first = received_message->topic;

            message_with_header& response = topicMessagePair.second;
            response.source_ip_address = client_addr.sin_addr.s_addr;
            response.source_port = client_addr.sin_port;
            memcpy(&response.msg, received_message, sizeof(message));

            waiting_messages.push(topicMessagePair);
        }

        // Check if a client wants to connect
        if (poll_fds[LISTNER_POLLFD_IDX].revents & POLLIN) {
            rc = connect_new_client(listenfd, poll_fds, subscribers, connected_subscribers);
            if (rc < 0)
                continue;
        }

        // Iterate through TCP clients
        for (size_t i = SUBS_START_IDX; i < poll_fds.size(); i++) {
            int clientfd = poll_fds[i].fd;
            std::string& client_name = connected_subscribers[clientfd];
            subscriber_data& subscriber = subscribers[client_name];

            // Check if the TCP client disconnected or sent something
            if (poll_fds[i].revents & POLLIN) {
                rc = recvall(clientfd, received_from_TCP, sizeof(subcribe_request));
                if (rc <= 0) {
                    std::cout << "Client " << client_name << " disconnected.\n";
                    poll_fds.erase(poll_fds.begin() + i);
                    connected_subscribers.erase(clientfd);

                    subscriber.connected = false;
                    shutdown(subscriber.sockfd, SHUT_RDWR);
                    close(subscriber.sockfd);
                    subscriber.sockfd = -1;

                    i--;
                } else {
                    process_subscription_requests(received_from_TCP, client_name, subscriber, clientfd);
                }
            }
        }

        // Send the next waiting message in the queue
        if (!waiting_messages.empty()) {
            auto& topicMessagePair = waiting_messages.front();

            for (auto& subscriber_pairs : subscribers) {
                subscriber_data& subscriber = subscriber_pairs.second;
                if (!subscriber.connected)
                    continue;

                for (size_t j = 0; j < subscriber.topics.size(); j++) {
                    if (matches_topic(topicMessagePair.first.c_str(), subscriber.topics[j].c_str())) {
                        char header = HEADER_MESSAGE;
                        rc = sendall(subscriber.sockfd, &header, 1);
                        if (rc < 0)
                            std::cerr << "Failed to send message header to " << subscriber_pairs.first << '\n';

                        rc = sendall(subscriber.sockfd, &topicMessagePair.second, sizeof(message_with_header));
                        if (rc < 0)
                            std::cerr << "Failed to send message to " << subscriber_pairs.first << '\n';
                    }
                }
            }
            waiting_messages.pop();
        }
    }

    shutdown(listenfd, SHUT_RDWR);
    close(listenfd);

    for (size_t i = SUBS_START_IDX; i < poll_fds.size(); i++) {
        shutdown(poll_fds[i].fd, SHUT_RDWR);
        close(poll_fds[i].fd);
    }
    
    close(udpfd);

    return 0;
}
