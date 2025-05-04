#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cmath>

void DIE(int condition, const char *message)
{
    if (condition) {
        std::cerr << "ERROR: " << message << "\n";
        exit(1);
    }
}

ssize_t sendall(int tcp_socket, const void* data, size_t size)
{
    size_t bytes_sent = 0;
    size_t bytes_remaining = size;

    int rc;
    while (bytes_remaining) {
        rc = send(tcp_socket, (const char*)data + bytes_sent, bytes_remaining, 0);
        if (rc < 0)
            return -1;
        
        bytes_sent += rc;
        bytes_remaining -= rc;
    }
    return bytes_sent;
}

ssize_t recvall(int tcp_socket, void* data, size_t size)
{
    size_t bytes_recv = 0;
    size_t bytes_remaining = size;

    int rc;
    while (bytes_remaining) {
        rc = recv(tcp_socket, (char*)data + bytes_recv, bytes_remaining, 0);

        // Check if the client disconnected gracefully
        if (rc == 0)
            return 0;
        
        // Check if the client disconnected abruptly
        if (rc < 0) {
            // Retry if interrupted by a signal
            if (errno == EINTR)
                continue;
            return -1;
        }
        
        bytes_recv += rc;
        bytes_remaining -= rc;
    }
    return bytes_recv;
}

void disable_nagle(int tcp_socket)
{
    int enable_option = 1;
    int rc = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY,
                        &enable_option, sizeof(int));
    DIE(rc < 0, "Failed to disable Nagle's algorithm");
}

std::string message::get_data_type()
{
    switch (data_type)
    {
    case DATA_TYPE_INT:
        return "INT";
    case DATA_TYPE_SHORT_REAL:
        return "SHORT_REAL";
    case DATA_TYPE_FLOAT:
        return "FLOAT";
    case DATA_TYPE_STRING:
        return "STRING";  
    }
    return "INVALID";
}

std::string message::get_displayed_content()
{
    uint8_t sign;
    uint32_t number_network_order;
    uint32_t number_host_order;

    uint16_t number_times_100;
    uint8_t exponent;
    float float_result;

    bool last_char_non_zero;

    std::stringstream ss;

    switch (data_type) {
    case DATA_TYPE_INT:
        sign = content[0];
        memcpy(&number_network_order, &content[1], 4);
        number_host_order = ntohl(number_network_order);

        if (sign == 1)
            ss << "-";
        ss << number_host_order;

        return ss.str();
    
    case DATA_TYPE_SHORT_REAL:
        memcpy(&number_times_100, &content[0], 2);

        ss << std::setprecision(2) << (float)number_times_100 / 100.f;
        break;
    
    case DATA_TYPE_FLOAT:
        sign = content[0];
        memcpy(&number_network_order, &content[1], 4);
        number_host_order = ntohl(number_network_order);
        exponent = content[5];

        float_result = (float)number_host_order * std::pow(10, -exponent);
        if (sign == 1)
            ss << "-";
        ss << std::setprecision(exponent) << float_result;
        break;

    case DATA_TYPE_STRING:
        last_char_non_zero = true;
        for (size_t i = 0; i < MAX_CONTENT_LENGTH; i++) {
            if (content[i] == '\0') {
                last_char_non_zero = false;
                break;
            }
        }
        if (last_char_non_zero) {
            char temp = content[MAX_CONTENT_LENGTH-1];
            content[MAX_CONTENT_LENGTH-1] = '\0';
            ss << content << temp;
            content[MAX_CONTENT_LENGTH-1] = temp;
        } else {
            ss << content;
        }
        break;
    default:
        ss << "INVALID";
    }
    return ss.str();
}

