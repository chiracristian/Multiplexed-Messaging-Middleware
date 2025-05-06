#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
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
    std::stringstream ss;

    switch (data_type) {
    case DATA_TYPE_INT: {
        uint8_t sign = content[0];

        uint32_t number;
        memcpy(&number, &content[1], 4);
        number = ntohl(number);

        if (sign == 1 && number != 0)
            ss << '-';
        ss << number;

        break;
    }
    
    case DATA_TYPE_SHORT_REAL: {
        uint16_t number_times_100;
        memcpy(&number_times_100, &content[0], 2);
        number_times_100 = ntohs(number_times_100);

        ss << number_times_100 / 100 << '.';
        ss << std::setw(2) << std::setfill('0') << number_times_100 % 100;
        break;
    }
    case DATA_TYPE_FLOAT: {
        uint8_t sign = content[0];

        uint32_t mantissa;
        memcpy(&mantissa, &content[1], 4);
        mantissa = ntohl(mantissa);

        uint8_t exponent = content[5];

        uint32_t power_of_10 = 1;
        for (uint32_t i = 0; i < exponent; i++)
            power_of_10 *= 10;

        if (sign == 1)
            ss << '-';
        ss << mantissa / power_of_10 << '.';
        ss << std::setw(exponent) << std::setfill('0') << mantissa % power_of_10;
        break;
    }
    case DATA_TYPE_STRING:
        ss << content;
        break;

    default:
        ss << "INVALID";
    }
    return ss.str();
}

