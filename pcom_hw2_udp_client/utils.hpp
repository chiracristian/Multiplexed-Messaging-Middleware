#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <string>

void DIE(int condition, const char *message);

#define MAX_TOPIC_LENGTH 50
#define MAX_CONTENT_LENGTH 1500

struct message_datagram {
    char topic[MAX_TOPIC_LENGTH];
    uint8_t data_type;
    char content[MAX_CONTENT_LENGTH];

    std::string get_data_type();
    std::string get_displayed_content();
};

#endif  // UTILS_H_INCLUDED
