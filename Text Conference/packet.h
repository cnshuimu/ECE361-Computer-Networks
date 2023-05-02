#include <stdlib.h>

#define MAX_NAME 32
#define MAX_DATA 800
#define BUFFER_SIZE 1000

enum packetTypes
{
    LOGIN,
    LOG_ACK,
    LOG_NAK,
    EXIT,
    JOIN,
    JOIN_ACK,
    JOIN_NAK,
    LEAVE,
    CREATE,
    CR_ACK,
    MESSAGE,
    QUERY,
    QUERY_ACK
};

struct message
{
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

void packetToString(const struct Packet *packet, char *buffer)
{
    // Initialize the buffer to store the resulting string
    memset(buffer, 0, sizeof(char) * BUFFER_SIZE);

    // Append packet type to buffer
    sprintf(buffer, "%d:", packet->type);

    // Append packet size to buffer
    sprintf(buffer + strlen(buffer), "%d:", packet->size);

    // Append packet source to buffer
    sprintf(buffer + strlen(buffer), "%s:", packet->source);

    // Append packet data to buffer
    memcpy(buffer + strlen(buffer), packet->data, packet->size);
}

void stringToPacket(const char *str, struct Packet *dest_packet)
{
    int cursor = 0;
    char dataStore[MAX_DATA];

    memset(dataStore, 0, MAX_DATA);

    // Get type
    int delimiter_pos = strcspn(str + cursor, ":");
    memcpy(dataStore, str + cursor, delimiter_pos);
    dest_packet->type = atoi(dataStore);
    cursor += delimiter_pos + 1;

    // Get size
    delimiter_pos = strcspn(str + cursor, ":");
    memset(dataStore, 0, MAX_DATA);
    memcpy(dataStore, str + cursor, delimiter_pos);
    dest_packet->size = atoi(dataStore);
    cursor += delimiter_pos + 1;

    // Get source
    delimiter_pos = strcspn(str + cursor, ":");
    memcpy(dest_packet->source, str + cursor, delimiter_pos);
    dest_packet->source[delimiter_pos] = 0;
    cursor += delimiter_pos + 1;

    // Get data
    memcpy(dest_packet->data, str + cursor, dest_packet->size);
}
