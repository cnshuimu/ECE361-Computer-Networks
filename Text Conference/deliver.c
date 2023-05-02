#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include "helper.h"

#define QUEUE_SIZE 10
#define MAX_SESSION 10

client user_list[MAX_USER_SIZE];
session session_list[MAX_SESSION];

pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;

void add_user(char id[], char pwd[], int* sockfd){
    int socketfd = *sockfd;
    pthread_mutex_lock(&user_lock);
    /* Loop over user list and find available slot */
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected == false){
            /* create user info */
            user_list[i].connected = true;
            user_list[i].in_session = false;
            user_list[i].socketfd = socketfd;
            strncpy(user_list[i].id, (char *)id, MAX_SOURCE_SIZE);
            strncpy(user_list[i].pwd, (char *)pwd, MAX_DATA_SIZE);

            fprintf(stdout, "User [%s] Login!\n", id);
            pthread_mutex_unlock(&user_lock);
            return;
        }
    }
    fprintf(stdout, "ERROR: User is Full - cannot add more user! \n");
    pthread_mutex_unlock(&user_lock);
}

void delete_user(char id[]) {
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            user_list[i].connected = false;
            user_list[i].in_session = false;
            fprintf(stdout, "User [%s] Logout! \n", id);
            pthread_mutex_unlock(&user_lock);
            return;
        }
    }
    fprintf(stdout, "ERROR: No ID found for to-be-deleted user! \n");
    pthread_mutex_unlock(&user_lock);
}

void leave_session(char id[]){
    char session_id[MAX_DATA_SIZE];
    memset(session_id, 0, MAX_DATA_SIZE);

    /* first check if user is in session */
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            /* set info in user list and get session id */
            user_list[i].in_session = false;
            strcpy(session_id, user_list[i].session_id);
            memset(user_list[i].session_id, 0, MAX_DATA_SIZE);
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Remove user from session */
    pthread_mutex_lock(&session_lock);

    /* Loop to find existing session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, session_id) == 0){
                /* decrement count and delete user from list */
                session_list[i].user_count -= 1;

                for (int j = 0; j < MAX_USER_SIZE; j++){
                    if (strcmp(session_list[i].user_id_list[j], id) == 0){
                        memset(session_list[i].user_id_list[j], 0, MAX_SOURCE_SIZE);
                        session_list[i].socketfd_list[j] = -1;
                        fprintf(stdout, "User [%s] leaves Session [%s].\n", id, session_id);
                        break;
                    }
                }

                /* delete session if no user inside */
                if (session_list[i].user_count == 0)
                    fprintf(stdout, "Session [%s] deleted!\n", session_id);

                break;
            }
        }
    }
    pthread_mutex_unlock(&session_lock);
}

int create_session(char data[], char id[], int* sockfd){
    int index;
    int socketfd = *sockfd;

    /* First check if the user is in session */
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            /* if user has already been in session */
            if (user_list[i].in_session){
                // fprintf(stdout, "ERROR: User already in session - cannot create session! \n");
                pthread_mutex_unlock(&user_lock);
                return 1;
            }
            /* if not in session, go to create */
            else {
                index = i;
                break;
            }
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Find if there is existed session with the same name */
    pthread_mutex_lock(&session_lock);
    /* Loop to find existing session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, data) == 0){
                pthread_mutex_unlock(&session_lock);
                return 2;
            }
        }
    }

    /* Loop to find empty session slot */
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count == 0){
            /* Create a session with given id */
            strcpy(session_list[i].id, data);
            session_list[i].user_count += 1;
            
            /* join session */
            strcpy(session_list[i].user_id_list[0], id);
            session_list[i].socketfd_list[0] = socketfd;

            pthread_mutex_lock(&user_lock);
            user_list[index].in_session = true;
            strcpy(user_list[index].session_id, data);
            pthread_mutex_unlock(&user_lock);

            fprintf(stdout, "Session [%s] created! \n", data);
            pthread_mutex_unlock(&session_lock);
            return 0;
        }
    }
    fprintf(stdout, "ERROR: Session is Full - cannot create session! \n");
    pthread_mutex_unlock(&session_lock);
    return 3;
}

char* generate_list(){
    // Allocate memory for the output string
    int output_size = 0;
    bool has_session = false;

    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++) {
        if (user_list[i].connected == true)
            output_size += strlen(user_list[i].id) + 2;  // Add 2 for the prefix "\t"
    }
    pthread_mutex_unlock(&user_lock);

    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            output_size += strlen(session_list[i].id) + 2;  // Add 2 for the prefix "\t"
            has_session = true;
        }
    }
    pthread_mutex_unlock(&session_lock);

    // Add extra space for the prefix and suffix strings
    output_size += 28;  
    output_size += 31;
    char* output = (char*)malloc(output_size * sizeof(char));

    // Generate the output string
    pthread_mutex_lock(&user_lock);
    sprintf(output, "Here is the list of users:\n");
    for (int i = 0; i < MAX_USER_SIZE; i++) {
        if (user_list[i].connected == true){
            strcat(output, "\t");
            strcat(output, user_list[i].id);
            strcat(output, "\n");
        }
    }
    pthread_mutex_unlock(&user_lock);

    pthread_mutex_lock(&session_lock);
    if (has_session){
        strcat(output, "Here is the list of sessions:\n");
        for (int i = 0; i < MAX_SESSION; i++) {
            if (session_list[i].user_count != 0){
                strcat(output, "\t");
                strcat(output, session_list[i].id);
                strcat(output, "\n");
            }
        }
    }
    else {
        strcat(output, "There is no existing session.\n");
    }
    pthread_mutex_unlock(&session_lock);

    return output;
}

int join_session(char data[], char id[], int* sockfd){
    /* Find index of user id in list */
    int index;
    int socketfd = *sockfd;
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Find the session with given id */
    /* Loop to find session */
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            /* found the session -> add user in */
            if (strcmp(session_list[i].id, data) == 0){
                /* ERROR: session is full */
                if (session_list[i].user_count == MAX_USER_SIZE){
                    pthread_mutex_unlock(&session_lock);
                    return 1;
                }
                /* set session_list[i] and its user list */
                session_list[i].user_count += 1;
                for (int j = 0; j < MAX_USER_SIZE; j++){
                    if (strlen(session_list[i].user_id_list[j]) == 0){ // empty slot for user in session
                        strcpy(session_list[i].user_id_list[j], id);
                        session_list[i].socketfd_list[j] = socketfd;
                        break;
                    }
                }
                /* set user list [index] */
                pthread_mutex_lock(&user_lock);
                user_list[index].in_session = true;
                strcpy(user_list[index].session_id, data);
                pthread_mutex_unlock(&user_lock);

                pthread_mutex_unlock(&session_lock);
                fprintf(stdout, "User [%s] joins Session [%s].\n", id, data);
                return 0;
            }
        }
    }
    /* Session does not exist */
    pthread_mutex_unlock(&session_lock);
    return 2;
}

void broadcast(char data[], char id[]){
    /* modify the format of output message */
    /* [MSG from user_1]: this is message */
    char temp[MAX_BUFF_SIZE];
    memset(temp, 0, MAX_BUFF_SIZE);
    strcpy(temp, "[MSG from ");
    strcat(temp, id);
    strcat(temp, "]: ");
    strcat(temp, data);

    /* Create MSG packet to users */
    packet send_pkt;
    send_pkt.type = 11; // MESSAGE
    send_pkt.size = strlen(temp);
    strcpy(send_pkt.data, temp);

    char buf[MAX_BUFF_SIZE];
    memset(buf, 0, MAX_BUFF_SIZE);
    createPacket(&send_pkt, buf);

    /* Find the session id for broadcast */
    char session_id[MAX_DATA_SIZE];
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
            strcpy(session_id, user_list[i].session_id);
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    /* Loop to find session */
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].user_count != 0){
            if (strcmp(session_list[i].id, session_id) == 0){ // found the session
                /* broadcast to all its users in user list */
                for (int j = 0; j < MAX_USER_SIZE; j++){
                    if (strlen(session_list[i].user_id_list[j]) != 0 && 
                        strcmp(session_list[i].user_id_list[j], id) != 0){ // other available users
                        /* Send MSG to server */
                        if (send(session_list[i].socketfd_list[j], buf, MAX_BUFF_SIZE, 0) == -1){
                            fprintf(stdout, "ERROR: server broadcast message - send error. \n");
                            pthread_mutex_unlock(&session_lock);
                            return;
                        }
                    }
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&session_lock);
}

void *server_func(void *arg) {
    int *socketfd = arg;
    char buffer[MAX_BUFF_SIZE];
    char *cursor;
    char id[MAX_SOURCE_SIZE], data[MAX_DATA_SIZE];

    while (true) {
        int byte_recv;
        packet recv_pkt, send_pkt;

        memset(&buffer, 0, MAX_BUFF_SIZE);
        memset(&data, 0, MAX_DATA_SIZE);

        /* Receiving from socket */
        byte_recv = recv(*socketfd, buffer, MAX_BUFF_SIZE, 0);
        if(byte_recv == -1) {
            fprintf(stdout, "ERROR: server func recv. \n");
            pthread_exit(NULL);
        }
        else if (byte_recv == 0){
            fprintf(stdout, "0 bytes recv, a connection is closed \n");
            pthread_exit(NULL);
        }
        
        /* login request */
        readPacket(&recv_pkt, buffer);
        if (recv_pkt.type == 1){ // LOGIN
            /* Store id and password */
            strncpy(id, (char *)(recv_pkt.source), MAX_SOURCE_SIZE);
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA_SIZE);
            
            /* Check if ID exists in user list */
            pthread_mutex_lock(&user_lock);
            bool dup_user = false;
            for (int i = 0; i < MAX_USER_SIZE; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    dup_user = true;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            /* Send-packet Creation */
            /* NAK for duplicate user_id */
            if (dup_user){
                send_pkt.type = 3; // LO_NAK
                strcpy(send_pkt.source, id);
                strcpy(send_pkt.data, "NAK: Username exists, try another name!\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            /* else go ACK msg */
            else {
                send_pkt.type = 2; // LO_ACK
                strcpy(send_pkt.source, id);
                send_pkt.size = 0;

                add_user(id, data, socketfd);
            }
            
            /* Sending Packets to client */
            memset(&buffer, 0, MAX_BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - send error. \n");
                close(*socketfd);
                pthread_exit(NULL);
            }
        }
        else if (recv_pkt.type == 4) { // EXIT
            /* check if user is in session */
            bool is_in_session = false;
            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < MAX_USER_SIZE; i++){
                if (user_list[i].connected && strcmp(user_list[i].id, id) == 0){
                    // leave session before logout
                    if (user_list[i].in_session){
                        is_in_session = true;
                    } 
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (is_in_session) leave_session(id);

            /* delete user when logout */
            delete_user(id);
        }
        else if (recv_pkt.type == 5) { // JOIN
            /* Store id and join session */
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA_SIZE);
            int res = join_session(data, id, socketfd);

            /* Send-packet Creation */
            if (res == 0){
                send_pkt.type = 6; // JN_ACK
                send_pkt.size = 0;
            } 
            else if (res == 1){ // session full
                send_pkt.type = 7; // JN_NAK
                strcpy(send_pkt.data, "JN_NAK: Session [");
                strcat(send_pkt.data, data);
                strcat(send_pkt.data, "] is Full.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 2){ // session not exist
                send_pkt.type = 7; // JN_NAK
                char* temp;
                strcpy(send_pkt.data, "JN_NAK: Session [");
                strcat(send_pkt.data, data);
                strcat(send_pkt.data, "] does not exist.\n");
                send_pkt.size = strlen(send_pkt.data);
            }

            /* Sending Packets to client */
            memset(&buffer, 0, MAX_BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - NS_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 8) { // LEAVE_SESS
            leave_session(id);
        }
        else if (recv_pkt.type == 9) { // NEW_SESS
            /* Store id and create session */
            strncpy(data, (char *)(recv_pkt.data), MAX_DATA_SIZE);
            int res = create_session(data, id, socketfd);

            /* Send-packet Creation */
            if (res == 0){
                send_pkt.type = 10; // NS_ACK
                send_pkt.size = 0;
            } 
            else if (res == 1){ // user in session
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: User already in session.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 3){ // session full
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: Too many Sessions.\n");
                send_pkt.size = strlen(send_pkt.data);
            }
            else if (res == 2){ // session exists
                send_pkt.type = 14; // NS_NAK
                strcpy(send_pkt.data, "NS_NAK: Session name exists, try another name.\n");
                send_pkt.size = strlen(send_pkt.data);
            }

            /* Sending Packets to client */
            memset(&buffer, 0, MAX_BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - NS_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 12) { // QUERY
            /* Send-packet Creation */
            send_pkt.type = 13; // QU_ACK
            char* temp = generate_list();
            memset(&send_pkt.data, 0, send_pkt.size);
            strcpy(send_pkt.data, temp);
            send_pkt.size = strlen(send_pkt.data);

            /* Sending Packets to client */
            memset(&buffer, 0, MAX_BUFF_SIZE);
            createPacket(&send_pkt, buffer);
            if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "ERROR: server func - QU_ACK error. \n");
            }
        }
        else if (recv_pkt.type == 11) { // MESSAGE
            broadcast(recv_pkt.data, id);
        }
        else
            fprintf(stdout, "ERROR: server func - receive unexpected ACK. \n");
    } 
}

int main (int argc, char *argv[]) {
    if (argc != 2){
        printf("Usage: server <TCP listen port> \n");
        return -1;
    }

    /* Variable Init */
    int socketfd;
    bool connected = false;
    char* port = argv[1]; // int port = atoi(argv[1]);
    int yes = 1;

    struct addrinfo hint, *res;
    memset(&hint, 0, sizeof hint);
    hint.ai_family = AF_INET; // IPv4  
    hint.ai_socktype = SOCK_STREAM; // for TCP
    hint.ai_flags = AI_PASSIVE;     // auto fill ip

    /* Loop over user list and set default false bool */
    for (int i = 0; i < MAX_USER_SIZE; i++){
        user_list[i].connected = false;
        user_list[i].in_session = false;
    }

    /* Loop over session list and set user count to 0 */
    for (int i = 0; i < MAX_SESSION; i++){
        session_list[i].user_count = 0;
    }

    /* Connection to port */
    if (getaddrinfo(NULL, port, &hint, &res) != 0){
        fprintf(stdout, "ERROR: server getaddrinfo. \n");
        return -1;
    }

    /* Loop over res to find available connection */
    for(struct addrinfo *temp = res; temp != NULL; temp = temp->ai_next) {
        socketfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (socketfd == -1) {
            fprintf(stdout, "ERROR: server socket. \n");
            continue;
        }

        /* By setting the SO_REUSEADDR option, the program can bind to the port 
        without waiting for the previous connection to time out. */
        if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            close(socketfd);
            fprintf(stdout, "ERROR: server setsocketopt. \n");
            continue;
        }

        if (bind(socketfd, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(socketfd);
            fprintf(stdout, "ERROR: server bind. \n");
            continue;
        }

        connected = true;
        break; 
    }

    /* if no available connection in res */
    if (connected == false){
        fprintf(stdout, "ERROR: No available connection in res. \n");
        return -1;
    }

    /* TCP Listen on Port */
    if (listen(socketfd, QUEUE_SIZE) == -1) {
        fprintf(stdout, "ERROR: server listen. \n");
        return -1;
    }

    // loop for accepting messages
    while (true) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        int* new_socketfd = malloc(sizeof(int)); 
        *new_socketfd = accept(socketfd, (struct sockaddr *)&addr, &len);
        if (*new_socketfd == -1) {
            fprintf(stdout, "ERROR: server accept. \n");
            continue;
        }
        pthread_t thread;
        pthread_create(&thread, NULL, server_func, (void *)new_socketfd);
    }
    
    return 0;
}