#include <cstring>
#include <iostream>
#include <queue>
#include <vector>
#include <unordered_set>
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
    std::unordered_set<std::string> topics;
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
    if (subscribers.count(connecting_id) > 0 &&
        subscribers[connecting_id].connected) {
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

void process_sub_requests(char *received_data,
                          std::string& client_name,
                          subscriber_data& subscriber)
{
    subcribe_request* request = (subcribe_request*)received_data;

    if (request->operation == OPERATION_SUBSCRIBE) {
        // Check if the client is already subscribed to that topic
        if (subscriber.topics.count(request->topic) > 0)
            return;

        // Add the topic to client's subscription list
        subscriber.topics.insert(request->topic);

        // Send a byte to prepare the client for receiving an acknowledgement
        // for subscribing
        char header = HEADER_SUBSCRIBE_ACK;
        int rc = sendall(subscriber.sockfd, &header, 1);
        if (rc < 0)
            std::cerr << "Failed to send ack header to " << client_name << '\n';

        // Send back the acknowledgement of subscribing
        request->operation = OPERATION_ACK_SUB;
        rc = sendall(subscriber.sockfd, request, sizeof(subcribe_request));
        if (rc < 0) {
            std::cerr << "Failed to send subscribe ACK\n";
        }
    } else if (request->operation == OPERATION_UNSUBSCRIBE) {
        // Check if the client is currently subscribed to the given topic 
        if (subscriber.topics.count(request->topic) == 0)
            return;

        // Remove the topic from client's subscription list
        subscriber.topics.erase(request->topic);

        // Send a byte to prepare the client for receiving an acknowledgement
        // for unsubscribing
        char header = HEADER_SUBSCRIBE_ACK;
        int rc = sendall(subscriber.sockfd, &header, 1);
        if (rc < 0)
            std::cerr << "Failed to send ack header to " << client_name << '\n';

        // Send back the acknowledgement of unsubscribing
        request->operation = OPERATION_ACK_UNSUB;
        rc = sendall(subscriber.sockfd, request, sizeof(subcribe_request));
        if (rc < 0)
            std::cerr << "Failed to send unsubscribe ACK\n";
    } else {
        std::cerr << "Invalid request from a TCP client\n";
    }
}

bool matches_topic(std::string searched_topic, std::string subscribed_topics)
{
    // Declare necessary variables for tokenizing the strigns
    const char* delim = "/";
    char* sub_save_ptr;
    char* topic_save_ptr;
    char* subscription = subscribed_topics.data();
    char* topic_of_msg = searched_topic.data();

    // Start with getting the first component of the paths
    char* current_sub_token = strtok_r(subscription, delim, &sub_save_ptr);
    char* current_search_token = strtok_r(topic_of_msg, delim, &topic_save_ptr);

    // Go through each part of the subscribed topics
    while (current_sub_token != NULL) {
        if (strcmp(current_sub_token, "+") == 0) {
            // In case of +, just go to the next tokens
            current_sub_token = strtok_r(NULL, delim, &sub_save_ptr);
            current_search_token = strtok_r(NULL, delim, &topic_save_ptr);
        } else if (strcmp(current_sub_token, "*") == 0) {
            // Go to the next component of subscribed path
            current_sub_token = strtok_r(NULL, delim, &sub_save_ptr);

            // If the last component was *, the path matches
            if (current_sub_token == NULL)
                return true;
            
            // Iterate through parts of the searched topic until we find
            // a part equal to the current component of the subcribed path
            while (current_search_token != NULL) {
                if (strcmp(current_search_token, current_sub_token) == 0)
                    break;
                current_search_token = strtok_r(NULL, delim, &topic_save_ptr);
            }

            // If we reached the end and didn't find the required part
            // of the searched topic, the subscription doesn't match.
            if (current_search_token == NULL)
                return false;

            // Continue with the next tokens
            current_sub_token = strtok_r(NULL, delim, &sub_save_ptr);
            current_search_token = strtok_r(NULL, delim, &topic_save_ptr);
        } else if (strcmp(current_sub_token, current_search_token) != 0) {
            return false;
        } else {
            // If the tokens match, continue with the next ones
            current_sub_token = strtok_r(NULL, delim, &sub_save_ptr);
            current_search_token = strtok_r(NULL, delim, &topic_save_ptr);
        }
    }

    // If the subscription matches, the whole searched topic
    // should have been consumed
    if (current_search_token != NULL)
        return false;

    return true;
}

void send_msg_to_all_subs(std::pair<std::string, message_with_header>& topic_message_pair,
                          std::unordered_map<std::string, subscriber_data>& subscribers,
                          std::unordered_map<int, std::string>& connected_subscribers)
{
    // For each connected subscriber
    for (auto& sub_id_data_pair : connected_subscribers) {
        subscriber_data& subscriber = subscribers[sub_id_data_pair.second];

        // Send the message to the subcriber only if it matches any of
        // the subscribed topics
        for (const std::string& subscribed_topics : subscriber.topics) {
            if (matches_topic(topic_message_pair.first, subscribed_topics)) {
                // Send a byte to prepare the client for receiving a message
                char header = HEADER_MESSAGE;
                int rc = sendall(subscriber.sockfd, &header, 1);
                if (rc < 0) {
                    std::cerr << "Failed to send message header to ";
                    std::cerr << sub_id_data_pair.second << '\n';
                    break;
                }

                // Send the actual message to the client
                rc = sendall(subscriber.sockfd, &topic_message_pair.second,
                                sizeof(message_with_header));
                if (rc < 0) {
                    std::cerr << "Failed to send message to ";
                    std::cerr << sub_id_data_pair.second << '\n';
                }
                
                break;
            }
        }
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

    // A queue of waiting messages, with their topic as key
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

            // Receive the message
            int rc = recvfrom(udpfd, received_datagram, sizeof(received_datagram),
                            0, (sockaddr*)&client_addr, &client_addr_size);
            if (rc < 0) {
                std::cerr << "Failed to receive UDP datagram\n";
                continue;;
            }
            message* received_message = (message*)received_datagram;

            // Make a pair of the topic and of the message that will include headers
            std::pair<std::string, message_with_header> topic_message_pair;
            topic_message_pair.first = received_message->topic;

            // Create the packet that will be sent, containing the message and its origin
            message_with_header& response = topic_message_pair.second;
            response.source_ip_address = client_addr.sin_addr.s_addr;
            response.source_port = client_addr.sin_port;
            memcpy(&response.msg, received_message, sizeof(message));

            // Enqueue the topic-message pair
            waiting_messages.push(topic_message_pair);
        }

        // Check if a client wants to connect
        if (poll_fds[LISTNER_POLLFD_IDX].revents & POLLIN) {
            rc = connect_new_client(listenfd, poll_fds,
                                    subscribers, connected_subscribers);
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
                    // Remove the client's fd from the poll and connected clients table
                    poll_fds.erase(poll_fds.begin() + i);
                    connected_subscribers.erase(clientfd);

                    // Mark it as not connected
                    subscriber.connected = false;

                    // Close their socket
                    shutdown(subscriber.sockfd, SHUT_RDWR);
                    close(subscriber.sockfd);
                    subscriber.sockfd = -1;

                    // Print a message announcing their disconnect
                    std::cout << "Client " << client_name << " disconnected.\n";

                    i--;
                } else {
                    process_sub_requests(received_from_TCP, client_name, subscriber);
                }
            }
        }

        // Send the next waiting message in the queue
        if (!waiting_messages.empty()) {
            auto& topic_message_pair = waiting_messages.front();

            send_msg_to_all_subs(topic_message_pair, subscribers,
                                 connected_subscribers);

            waiting_messages.pop();
        }
    }

    // Close the TCP listner socket
    shutdown(listenfd, SHUT_RDWR);
    close(listenfd);

    // Close all TCP client sockets
    for (size_t i = SUBS_START_IDX; i < poll_fds.size(); i++) {
        shutdown(poll_fds[i].fd, SHUT_RDWR);
        close(poll_fds[i].fd);
    }
    
    // Finally, close the UDP socket
    close(udpfd);

    return 0;
}
