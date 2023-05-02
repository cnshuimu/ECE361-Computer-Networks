#ifndef helper_h
#define helper_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_BUFF_SIZE 1400
#define MAX_DATA_SIZE 1024
#define MAX_USER_SIZE 10
#define MAX_SOURCE_SIZE 128

#define LOGIN 1
#define LO_ACK 2
#define LO_NAK 3
#define EXIT  4 
#define JOIN 5 
#define JN_ACK 6
#define JN_NAK 7
#define LEAVE_SESS 8
#define NEW_SESS  9 
#define NS_ACK  10
#define MESSAGE 11
#define QUERY  12
#define QU_ACK 13
#define NS_NAK  14
#define REGISTER  15
#define RG_ACK  16
#define RG_NAK  17
#define PRIVATE 18


typedef struct message {
    unsigned int type;  
    unsigned int size; 
    unsigned char source[MAX_SOURCE_SIZE];
    unsigned char data[MAX_DATA_SIZE];
} packet;

typedef struct client{ 
    int socketfd;
    bool connection;
    bool sess_state;
    unsigned char id[MAX_SOURCE_SIZE];
    unsigned char password[MAX_DATA_SIZE]; 
    unsigned char sid[MAX_DATA_SIZE]; 
}client; 

typedef struct session{
    char id[MAX_DATA_SIZE];
    int num_of_user;
    char list_id[MAX_USER_SIZE][MAX_SOURCE_SIZE]; 
    int socket_list[MAX_USER_SIZE];
}session;

void StrToPack(packet *packet, char *buffer)
{
    memset(buffer, 0, sizeof(buffer));

    sprintf(buffer, "%d:", packet->type);

    sprintf(buffer + strlen(buffer), "%s:", packet->source);
    
    sprintf(buffer + strlen(buffer), "%d:", packet->size);

    memcpy(buffer + strlen(buffer), packet->data, packet->size);
}

void PackToStr(packet *packet, const char *buffer)
{
    int ptr = 0;
    char info[MAX_DATA_SIZE];

    memset(info, 0, sizeof(info));

    int delimiter_pos = strcspn(buffer + ptr, ":");
    memcpy(info, buffer + ptr, delimiter_pos);
    packet->type = atoi(info);
    ptr += delimiter_pos + 1;

    delimiter_pos = strcspn(buffer + ptr, ":");
    memset(info, 0, sizeof(info));
    memcpy(info, buffer + ptr, delimiter_pos);
    packet->size = atoi(info);
    ptr += delimiter_pos + 1;

    delimiter_pos = strcspn(buffer + ptr, ":");
    memcpy(packet->source, buffer + ptr, delimiter_pos);
    packet->source[delimiter_pos] = 0;
    ptr += delimiter_pos + 1;

    memcpy(packet->data, buffer + ptr, packet->size);
}


#endif