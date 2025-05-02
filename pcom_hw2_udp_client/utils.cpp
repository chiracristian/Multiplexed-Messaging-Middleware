#include "utils.hpp"

#include <arpa/inet.h>
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

std::string message_datagram::get_data_type()
{
    switch (data_type)
    {
    case 0:
        return "INT";
    case 1:
        return "SHORT_REAL";
    case 2:
        return "FLOAT";
    case 3:
        return "STRING";  
    }
    return "INVALID";
}

std::string message_datagram::get_displayed_content()
{
    std::stringstream ss;
    switch (data_type) {
    case 0:
        uint8_t sign = content[0];
        uint32_t number_network_order;
        memcpy(&number_network_order, &content[1], 4);
        uint32_t number_host_order = ntohl(number_network_order);

        if (sign == 1)
            ss << "-";
        ss << number_host_order;

        return ss.str();
    
    case 1:
        uint16_t number_times_100;
        memcpy(&number_times_100, &content[0], 2);

        float number = (float)number_times_100 / 100.f;
        ss << std::setprecision(2) << number;
        break;
    
    case 2:
        uint8_t sign = content[0];
        uint32_t number_network_order;
        memcpy(&number_network_order, &content[1], 4);
        uint32_t number_host_order = ntohl(number_network_order);
        uint8_t exponent = content[5];

        float result = (float)number_host_order * std::pow(10, -exponent);
        if (sign == 1)
            ss << "-";
        ss << std::setprecision(exponent) << result;
        break;

    case 3:
        bool last_char_non_zero = true;
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

