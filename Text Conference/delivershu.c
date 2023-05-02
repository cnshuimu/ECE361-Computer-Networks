#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "commands.h"
#include "packet.h"

#define SUCCESS 0
#define UNINITIALIZED -1
#define BUFFER_SIZE 1000

// Input buffer
char BUFFER[BUFFER_SIZE];

/*
Client currently in a session - true
Client currently not in a session - false
*/
bool status = false;

void *getIPAddress(struct sockaddr *socketAddress)
{
    if (socketAddress->sa_family == AF_INET)
    {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)socketAddress;
        return &(ipv4->sin_addr);
    }
    else
    {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)socketAddress;
        return &(ipv6->sin6_addr);
    }
}

void *receive(void *arg)
{
    // Cast the argument to the socket file descriptor
    int *sockfd = (int *)arg;

    // Packet for storing received data
    struct Packet packet;

    // Loop to continuously receive packets from the server
    for (;;)
    {
        // Receive packet from server
        int numbytes = recv(sockfd, BUFFER, BUFFER_SIZE - 1, 0);

        // Check for errors
        if (numbytes == -1)
        {
            printf("Error: Cannot receive packet from server.\n");
            return NULL;
        }

        // Check if no data was received
        if (numbytes == 0)
            continue;

        // Add null terminator to received data
        BUFFER[numbytes] = 0;

        // Parse received data into a packet
        stringToPacket(BUFFER, &packet);

        // Handle received packet based on its type
        switch (packet.type)
        {
        case JOIN_ACK:
            printf("Successfully joined session %s.\n", packet.data);
            status = true;
        case JOIN_NAK:
            printf("Failed to join the session. Details: %s\n", packet.data);
            status = false;
        case CR_ACK:
            printf("Successfully created and joined session %s.\n", packet.data);
            status = true;
        case QUERY_ACK:
            printf("User id\t\tSession ids\n%s", packet.data);
        case MESSAGE:
            printf("%s: %s\n", packet.source, packet.data);
        default:
            printf("Unexpected packet received: type %d, data %s\n", packet.type, packet.data);
        }

        // Flush stdout to prevent ghost newlines
        fflush(stdout);
    }

    // We should never reach this point, but return NULL to satisfy the compiler
    return NULL;
}

void login(char *input, int *socketFileDescriptor, pthread_t *receive_thread)
{
    // Check if the socketfd is intialized or not
    if (*socketFileDescriptor != UNINITIALIZED)
    {
        printf("You have already logged in to a server.\n");
        return;
    }

    // Get login information
    char *clientID = strtok(NULL, " ");
    char *pwd = strtok(NULL, " ");
    char *serverIP = strtok(NULL, " ");
    char *serverPort = strtok(NULL, " ");

    // Check if information is complete
    if (clientID == NULL || pwd == NULL || serverIP == NULL || serverPort == NULL)
    {
        printf("Error: Please enter '/login <client ID> <password> <server-IP> <server-port>'\n");
        return;
    }

    // All good, ready to setup TCP connection
    int addrinfo;
    struct addrinfo spec, *serverInfo, *curAddr;
    char s[INET6_ADDRSTRLEN];
    memset(&spec, 0, sizeof spec);
    spec.ai_family = AF_UNSPEC;
    spec.ai_socktype = SOCK_STREAM;

    // Get server address information
    if ((addrinfo = getaddrinfo(serverIP, serverPort, &spec, &serverInfo)) != 0)
    {
        printf("Error: Cannot get server address information.\n");
        return;
    }

    // Try to setup socket and establish connection
    for (curAddr = serverInfo; curAddr != NULL; curAddr = curAddr->ai_next)
    {
        if ((*socketFileDescriptor = socket(curAddr->ai_family, curAddr->ai_socktype, curAddr->ai_protocol)) == -1)
        {
            printf("Error: Failed to create a socket.\n");
            continue;
        }
        if (connect(*socketFileDescriptor, curAddr->ai_addr, curAddr->ai_addrlen) == -1)
        {
            close(*socketFileDescriptor);
            printf("Error: Failed to establish a connection.\n");
            continue;
        }
        break;
    }

    if (curAddr == NULL)
    {
        printf("Error: Failed to connect to server.\n");
        close(*socketFileDescriptor);
        *socketFileDescriptor = UNINITIALIZED;
        return;
    }

    inet_ntop(curAddr->ai_family, getIPAddress((struct sockaddr *)curAddr->ai_addr), s, sizeof s);
    printf("Connecting to %s...\n", s);
    freeaddrinfo(serverInfo);

    // Create LOGIN packet
    int numbytes;
    struct Packet packet;
    packet.type = LOGIN;
    strncpy(packet.source, clientID, MAX_SOURCE);
    strncpy(packet.data, pwd, MAX_DATA_SIZE);
    packet.size = strlen(packet.data);
    packetToString(&packet, BUFFER);

    // Send LOGIN packet to server
    if (numbytes = send(*socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0) == -1)
    {
        printf("Error: Cannot send LOGIN packet.\n");
        close(*socketFileDescriptor);
        *socketFileDescriptor = UNINITIALIZED;
        return;
    }

    // Receive LOG_ACK from server
    if (numbytes = recv(*socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0) == -1)
    {
        printf("Error: Cannot receive LOG_ACK from server.\n");
        close(*socketFileDescriptor);
        *socketFileDescriptor = UNINITIALIZED;
        return;
    }

    BUFFER[numbytes] = 0;
    stringToPacket(BUFFER, &packet);
    if (packet.type == LOG_ACK && pthread_create(receive_thread, NULL, receive, socketFileDescriptor) == 0)
    {
        printf("You are logged in!\n");
        return;
    }
    else if (packet.type == LOG_NAK)
    {
        printf("Failed to login.\n");
        close(*socketFileDescriptor);
        *socketFileDescriptor = UNINITIALIZED;
        return;
    }
    else
    {
        printf("Unexpected packet received: type %d, data %s\n", packet.type, packet.data);
        close(*socketFileDescriptor);
        *socketFileDescriptor = UNINITIALIZED;
        return;
    }
}

void logout(int *socketFileDescriptor, pthread_t *receive_thread)
{
    // Check if the socketfd is intialized or not
    if (*socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }

    // Create EXIT packet
    struct Packet packet;
    packet.type = EXIT;
    packet.size = 0;
    packetToString(&packet, BUFFER);

    // Try to send EXIT packet to server
    int numbytes = send(*socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send EXIT packet.\n");
        return;
    }
    if (pthread_cancel(*receive_thread))
    {
        printf("Error: Logout failed.\n");
    }
    else
    {
        printf("Logout successful");
    }
    status = false;
    close(*socketFileDescriptor);
    *socketFileDescriptor = UNINITIALIZED;
}

void join(char *input, int *socketFileDescriptor)
{
    // Check if the socketfd is intialized or not
    if (*socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }
    else if (status)
    {
        printf("You have already joined a session.\n");
        return;
    }

    char *sessionID = strtok(NULL, " ");
    if (sessionID == NULL)
    {
        printf("Error: Please enter '/joinsession <session ID>'");
        return;
    }

    // Create JOIN packet
    struct Packet packet;
    packet.type = JOIN;
    strncpy(packet.data, sessionID, MAX_DATA_SIZE);
    packet.size = strlen(packet.data);
    packetToString(&packet, BUFFER);

    // Try to send JOIN packet to server
    int numbytes = send(*socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send JOIN packet to server.\n");
        return;
    }
}

void leave(int socketFileDescriptor)
{
    // Check if the socketfd is intialized or not
    if (socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }
    else if (!status)
    {
        printf("You have not joined any session.\n");
        return;
    }

    // Create LEAVE packet
    struct Packet packet;
    packet.type = LEAVE;
    packet.size = 0;
    packetToString(&packet, BUFFER);

    // Try to send LEAVE packet to server
    int numbytes = send(socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send LEAVE packet to server.\n");
    }
}

void create(int socketFileDescriptor)
{
    // Check if the socketfd is intialized or not
    if (socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }
    else if (status)
    {
        printf("You are in a session, cannot create new sessions right now.\n");
        return;
    }

    // Create CREATE packet
    struct Packet packet;
    packet.type = CREATE;
    packet.size = 0;
    packetToString(&packet, BUFFER);

    // Try to send CREATE packet to server
    int numbytes = send(socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send CREATE packet to server.\n");
    }
}

void list(int socketFileDescriptor)
{
    // Check if the socketfd is intialized or not
    if (socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }

    // Create QUERY packet
    struct Packet packet;
    packet.type = QUERY;
    packet.size = 0;
    packetToString(&packet, BUFFER);

    // Try to send QUERY packet to server
    int numbytes = send(socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send QUERY packet to server.\n");
    }
}

void sendText(int socketFileDescriptor)
{
    // Check if the socketfd is intialized or not
    if (socketFileDescriptor == UNINITIALIZED)
    {
        printf("You haven't logged in to any server.\n");
        return;
    }
    else if (!status)
    {
        printf("You have not joined any session.\n");
        return;
    }

    // Create MESSAGE packet
    struct Packet packet;
    packet.type = MESSAGE;
    strncpy(packet.data, BUFFER, MAX_DATA_SIZE);
    packet.size = strlen(packet.data);
    packetToString(&packet, BUFFER);

    // Try to send MESSAGE packet to server
    int numbytes = send(socketFileDescriptor, BUFFER, BUFFER_SIZE - 1, 0);
    if (numbytes == -1)
    {
        printf("Error: Cannot send MESSAGE packet to server.\n");
    }
}

int main()
{
    char *input;
    int input_len;
    int socketFileDescriptor = UNINITIALIZED;
    pthread_t receive_thread;

    while (true)
    {
        // Prompt user for input
        printf("> ");

        // Get input
        fgets(BUFFER, BUFFER_SIZE - 1, stdin);
        BUFFER[strcspn(BUFFER, "\n")] = 0;
        input = BUFFER;

        while (*input == ' ')
            input++;
        if (*input == 0)
        {
            // ignore empty command
            continue;
        }
        input = strtok(BUFFER, " ");
        input_len = strlen(input);

        if (strcmp(input, LOGIN_CMD) == 0)
        {
            printf("Logging you in...\n");
            login(input, &socketFileDescriptor, &receive_thread);
        }
        else if (strcmp(input, LOGOUT_CMD) == 0)
        {
            printf("Logging you out...\n");
            logout(&socketFileDescriptor, &receive_thread);
        }
        else if (strcmp(input, JOIN_CMD) == 0)
        {
            printf("Join you in...\n");
            join(input, &socketFileDescriptor);
        }
        else if (strcmp(input, LEAVE_CMD) == 0)
        {
            printf("Leaving session...\n");
            leave(socketFileDescriptor);
        }
        else if (strcmp(input, CREATE_CMD) == 0)
        {
            printf("Creating session...\n");
            create(socketFileDescriptor);
        }
        else if (strcmp(input, LIST_CMD) == 0)
        {
            printf("Session list: \n");
            list(socketFileDescriptor);
        }
        else if (strcmp(input, QUIT_CMD) == 0)
        {
            // Exit program
            logout(&socketFileDescriptor, &receive_thread);
            break;
        }
        else
        {
            // Send text
            sendText(socketFileDescriptor);
        }
    }

    printf("You have quit successfully.\n");
    return SUCCESS;
}