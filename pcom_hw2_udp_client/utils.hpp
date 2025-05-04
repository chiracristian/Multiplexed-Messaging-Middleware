#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <string>

void DIE(int condition, const char *message);

#define HEADER_MESSAGE 0
#define HEADER_SUBSCRIBE_ACK 1

ssize_t sendall(int tcp_socket, const void* data, size_t size);
ssize_t recvall(int tcp_socket, void* data, size_t size);
void disable_nagle(int tcp_socket);

#define ID_LENGTH 10

#define OPERATION_REQUIRE_CONNECTION 0
#define OPERATION_IDENTICAL_ID_DETECTED 1

// struct connection_request {
//     uint8_t operation;
//     char id[ID_LENGTH];
// };

#define MAX_TOPIC_LENGTH 50
#define MAX_CONTENT_LENGTH 1500

#define DATA_TYPE_INT 0
#define DATA_TYPE_SHORT_REAL 1
#define DATA_TYPE_FLOAT 2
#define DATA_TYPE_STRING 3

struct message {
    char topic[MAX_TOPIC_LENGTH];
    uint8_t data_type;
    char content[MAX_CONTENT_LENGTH];

    std::string get_data_type();
    std::string get_displayed_content();
};

struct message_with_header {
    uint32_t source_ip_address;
    uint16_t source_port;
    message msg;
};

#define OPERATION_SUBSCRIBE 0
#define OPERATION_UNSUBSCRIBE 1
#define OPERATION_ACK_SUB 2
#define OPERATION_ACK_UNSUB 3

struct subcribe_request {
    uint8_t operation;
    char topic[MAX_TOPIC_LENGTH+1];
};

#endif  // UTILS_H_INCLUDED
