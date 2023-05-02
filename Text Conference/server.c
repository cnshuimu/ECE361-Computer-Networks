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
#define MAX_SESSION 10
#define QUEUE_SIZE 10
#define MAX_STRLEN 500
// setup multi-thread
pthread_mutex_t sessionlist_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientlist_mutex = PTHREAD_MUTEX_INITIALIZER;


client client_list[MAX_USER_SIZE];
session session_list[MAX_SESSION];


int reg_user(char* uid, char* pwd, char* file){
    int num_users = 0;
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        fprintf(stdout, "CANNOT READ FILE\n", file);
        return false;
    }

    client regester_user[MAX_USER_SIZE];
    unsigned char buff[MAX_BUFF_SIZE];
    unsigned char reg_username[MAX_SOURCE_SIZE];
    unsigned char reg_password[MAX_DATA_SIZE];
    memset(buff, 0, MAX_BUFF_SIZE);

    while (fgets(buff, MAX_BUFF_SIZE, fp) != NULL) {
        memset(reg_username, 0, MAX_SOURCE_SIZE);
        memset(reg_password, 0, MAX_DATA_SIZE);
        sscanf(buff, "%s %s", reg_username, reg_password);
        strncpy(regester_user[num_users].id, reg_username, sizeof(reg_username));
        strncpy(regester_user[num_users].password, reg_password, sizeof(reg_password)); 
    }

    fclose(fp);
    for (int i = 0; i < num_users; i++) {
        if (strcmp(regester_user[i].id, uid) == 0){
            if(strcmp(regester_user[i].password, pwd)){
                return 0;
            }
            else{
                return 1;
            }
        } 
    }
    fp = fopen(file, "a"); 
    fprintf(fp, "%s %s\n", uid, pwd);
    fclose(fp);

    return 3;
}


char* serverlist(){

    pthread_mutex_lock(&sessionlist_mutex);
    char* showlist = (char*)malloc(MAX_STRLEN * sizeof(char));
    // Generate the output string
    sprintf(showlist, "----- USER LIST-----\n");
    for (int i = 0; i < MAX_USER_SIZE; i++) {
        if (client_list[i].connection == true){
            strcat(showlist, client_list[i].id);
            strcat(showlist, "\n");
        }
    }

    strcat(showlist, "-----SESSION LIST-----\n");
    for (int i = 0; i < MAX_SESSION; i++) {
        if (session_list[i].num_of_user != 0){
            strcat(showlist, session_list[i].id);
            strcat(showlist, "\n");
        }
    }
    pthread_mutex_unlock(&sessionlist_mutex);
    return showlist;
}


void add_user(char uid[], char password[], int* socket){
    bool isfull = true;

    // set the multi thread lock
    pthread_mutex_lock(&clientlist_mutex);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (client_list[i].connection == false){
            
            client_list[i].socketfd = *socket;
            client_list[i].connection = true;
            strncpy(client_list[i].id, uid, MAX_SOURCE_SIZE);
            strncpy(client_list[i].password, password, MAX_DATA_SIZE);
            
            //output user id in the server
            fprintf(stdout, "User Login: %s\n", uid);
            isfull = false;
            break;
        }
    }
    if (isfull){
        fprintf(stdout, "Client list full \n");
    }
    
    pthread_mutex_unlock(&clientlist_mutex);
    return;
}


void delete_user(char uid[]) {
    bool flag = true;
    //put lock on
    pthread_mutex_lock(&clientlist_mutex);

    //check if the user is connect to the server and in the list, then logout the user
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (client_list[i].connection == true && strcmp(client_list[i].id, uid) == 0){

            client_list[i].sess_state = false;
            client_list[i].connection = false;

            fprintf(stdout, "User Logout: %s \n", uid);
            flag = false;
            break;
        }
    }
    if(flag) fprintf(stdout, "ERROR: No ID found for to-be-deleted user! \n");
    pthread_mutex_unlock(&clientlist_mutex);
    return;
}


int create_session(char data[], char uid[], int* socketfd){

    int socket = *socketfd;
    int sess_num;
    pthread_mutex_lock(&sessionlist_mutex);

    //check if the user is in session 
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (strcmp(client_list[i].id, uid) == 0){
            if(client_list[i].sess_state == true){
                pthread_mutex_unlock(&sessionlist_mutex);
                //give server commond 1 for NS_NAK
                return 1;
            }
            sess_num = i;
            break;
        }        
    }

    //create session
    for (int i = 0; i < MAX_SESSION; i++) {
        //check if session already exit in session list
        if( strcmp(session_list[i].id, data) == 0 ){
        //if (strcmp(session_list[i].id, data) == 0 ){
            pthread_mutex_unlock(&sessionlist_mutex);
            return 2;    
        }
       else if (session_list[i].num_of_user == 0){
            //update session state in client info
            client_list[sess_num].sess_state = true;
            strcpy(client_list[sess_num].sid, data);
            //create session,automatic put the use into the session
            strcpy(session_list[i].id, data);
            strcpy(session_list[i].list_id[0], uid);
            session_list[i].socket_list[0] = socket;
            session_list[i].num_of_user += 1;
            fprintf(stdout, "Created Session: %s \n", data);
            pthread_mutex_unlock(&sessionlist_mutex);
            break;
        }
        
    }
    return 0;
}

int join_session(char data[], char uid[], int* sockfd){
    int socket = *sockfd;
    //find the given session
    pthread_mutex_lock(&sessionlist_mutex);
    for (int i = 0; i < MAX_SESSION; i++) {
        if (strcmp(session_list[i].id, data) == 0){
            // section full
            if (session_list[i].num_of_user == MAX_USER_SIZE){
                pthread_mutex_unlock(&sessionlist_mutex);
                return 1;
            }
            for (int j = 0; j < MAX_USER_SIZE; j++){
                //find the user and the avaiable space in session list
                if (strlen(session_list[i].list_id[j]) == 0 && strcmp(client_list[j].id, uid) == 0){
                    client_list[j].sess_state = true;
                    strcpy(client_list[j].sid, data);
                    strcpy(session_list[i].list_id[j], uid);
                    session_list[i].socket_list[j] = socket;
                    break;   
                }
            }
            session_list[i].num_of_user += 1;
            fprintf(stdout, "User: %s -> joins Session: %s \n", uid, data);
            return 0;
        }
    }
    // no given session exist 
    pthread_mutex_unlock(&sessionlist_mutex);
    return 2;
}

void leave_session(char uid[]){
    // char sid[MAX_DATA_SIZE];
    // memset(sid, 0, MAX_DATA_SIZE);

    pthread_mutex_lock(&sessionlist_mutex);
    //disconnect the user from session's side
    for (int i = 0; i < MAX_SESSION; i++) {
        for (int j = 0; j < MAX_USER_SIZE; j++){
            if (strcmp(session_list[i].list_id[j], uid) == 0){
                session_list[i].socket_list[j] = -1;
                session_list[i].num_of_user -= 1;
                fprintf(stdout, "User: %s -> leaves Session: %s \n", uid, session_list[i].id);
                memset(session_list[i].list_id[j], 0, MAX_SOURCE_SIZE);
                break;
            }
        }
    }

    //disconnect the session from user's side
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if (strcmp(client_list[i].id, uid) == 0){
            /* set info in user list and get session id */
            client_list[i].sess_state = false;
            // strcpy(sid, client_list[i].sid);
            memset(client_list[i].sid, 0, MAX_DATA_SIZE);
            break;
        }
    }
    pthread_mutex_unlock(&clientlist_mutex);
}


void broadcast(char data[], char id[]){
    char mesg[MAX_BUFF_SIZE];
    memset(mesg, 0, MAX_BUFF_SIZE);
    strcpy(mesg, "MESSAGE FROM ");
    strcat(mesg, id);
    strcat(mesg, " : ");
    strcat(mesg, data);

    //message
    packet packetToUser;
    packetToUser.type = MESSAGE; 
    packetToUser.size = strlen(mesg);
    strcpy(packetToUser.data, mesg);

    char buf[MAX_BUFF_SIZE];
    memset(buf, 0, MAX_BUFF_SIZE);
    StrToPack(&packetToUser, buf);

    char sid[MAX_DATA_SIZE];

    pthread_mutex_lock(&clientlist_mutex);
    for (int i = 0; i < MAX_SESSION; i++) {
        for (int j = 0; j < MAX_USER_SIZE; j++){
            if (client_list[j].connection && strcmp(client_list[j].id, id) == 0 && strcmp(session_list[i].id, client_list[j].sid )){
                if (strlen(session_list[i].list_id[j]) != 0 && strcmp(session_list[i].list_id[j], id) != 0){ // other available users
                    if (send(session_list[i].socket_list[j], buf, MAX_BUFF_SIZE, 0) == -1){
                        fprintf(stdout, "SERVER BOARDCAST ERROR \n");
                        return;
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&sessionlist_mutex);
}

void private_broadcast(char data[], char id[], char tar_id[]){
    char temp[MAX_BUFF_SIZE];
    memset(temp, 0, MAX_BUFF_SIZE);
    strcpy(temp, "PRIVATE MESSAGE FROM ");
    strcat(temp, id);
    strcat(temp, " : ");
    strcat(temp, data);

    packet packetToUser;
    packetToUser.type = 11; // MESSAGE
    packetToUser.size = strlen(temp);
    strcpy(packetToUser.data, temp);

    char buf[MAX_BUFF_SIZE];
    memset(buf, 0, MAX_BUFF_SIZE);
    StrToPack(&packetToUser, buf);

    pthread_mutex_lock(&sessionlist_mutex);
    for (int i = 0; i < MAX_USER_SIZE; i++){
        if(strcmp(client_list[i].id, tar_id) == 0){
            if (send(client_list[i].socketfd, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "PRIVATE MESSAGE SEND FAILED \n");
                pthread_mutex_unlock(&sessionlist_mutex);
                return;
            }
            else{
                pthread_mutex_unlock(&sessionlist_mutex);
                fprintf(stdout, "PRIVATE MESSAGE SENDED \n");
                return;
            }
        }
    }
    pthread_mutex_unlock(&sessionlist_mutex);
    fprintf(stdout, "ERROR: TARGET DOES NOT EXIST \n");
    return;
}


void *server_func(void *arg) {
    bool same_user = false;
    int *socket = arg;
    char *cursor;
    char buf[MAX_BUFF_SIZE],id[MAX_SOURCE_SIZE], data[MAX_DATA_SIZE],target_id[MAX_SOURCE_SIZE];

    while (true) {
        memset(&buf, 0, MAX_BUFF_SIZE);
        memset(&data, 0, MAX_DATA_SIZE);
        memset(&target_id, 0, MAX_SOURCE_SIZE);
        packet packetRev, packetToUser;
        
        int rev_from_client;
        //listening from socket
        rev_from_client = recv(*socket, buf, MAX_BUFF_SIZE, 0);

        if (rev_from_client == 0){
            fprintf(stdout, "RECIVE 0 Byte: connection is closed \n");
            pthread_exit(NULL);
        }

        if(rev_from_client == -1) {
            fprintf(stdout, "SERVER FUNCTION ERROR: reciving from socket error \n");
            pthread_exit(NULL);
        }

        //handle login 
        PackToStr(&packetRev, buf);

        if (packetRev.type == LOGIN){ 
            strncpy(id, (char *)(packetRev.source), MAX_SOURCE_SIZE);
            strncpy(data, (char *)(packetRev.data), MAX_DATA_SIZE);
            int regist = reg_user("registerlist.txt", id, data);
            
            pthread_mutex_lock(&clientlist_mutex);

            for (int i = 0; i < MAX_USER_SIZE; i++){
                if (strcmp(client_list[i].id, id) == 0){
                    same_user = true;
                    break;
                }
            }
            pthread_mutex_unlock(&clientlist_mutex);
            if (regist == 1){
                packetToUser.type = 3; 
                strcpy(packetToUser.source, id);
                strcpy(packetToUser.data, "**LO_NAK** : Wrong Password\n");
                packetToUser.size = strlen(packetToUser.data);
                memset(&id, 0, MAX_SOURCE_SIZE);
            }
            
            else if (regist == 2){
                packetToUser.type = 3; 
                strcpy(packetToUser.source, id);
                strcpy(packetToUser.data, "**LO_NAK**: Username Not Register\n");
                packetToUser.size = strlen(packetToUser.data);
                memset(&id, 0, MAX_SOURCE_SIZE);
            }
            if (same_user){
                packetToUser.type = LO_NAK; 
                strcpy(packetToUser.data, "**LO_NAK** : Duplicate Username\n");
                strcpy(packetToUser.source, id);
                packetToUser.size = strlen(packetToUser.data);
            }
            
            else {
                packetToUser.type = LO_ACK; // LO_ACK
                strcpy(packetToUser.source, id);
                packetToUser.size = 0;
                add_user(id, data, socket);
            }
            //packet to client
            memset(&buf, 0, MAX_BUFF_SIZE);
            StrToPack(&packetToUser, buf);

            if (send(*socket, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "SERVER FUNCTION ERROR: sent to client error \n");
                close(*socket);
                pthread_exit(NULL);
            }
        }
        if (packetRev.type == EXIT) { 
            // find if user already in a session 
            pthread_mutex_lock(&clientlist_mutex);
            bool is_sess_state = false;
            for (int i = 0; i < MAX_USER_SIZE; i++){
                if (client_list[i].connection && strcmp(client_list[i].id, id) == 0){
                    // leave session before logout
                    if (client_list[i].sess_state){
                        is_sess_state = true;
                    } 
                    break;
                }
            }
            pthread_mutex_unlock(&clientlist_mutex);

            if (is_sess_state){
                leave_session(id);
            }
            delete_user(id);
        }
        if (packetRev.type == JOIN) {
            strncpy(data, (char *)(packetRev.data), MAX_DATA_SIZE);
            int res = join_session(data, id, socket);

            if (res == 0){
                packetToUser.type = JN_ACK; 
                packetToUser.size = 0;
            } 
            else if (res == 1){ 
                packetToUser.type = JN_NAK; 
                strcpy(packetToUser.data, "**JN_NAK**: Session: ");
                strcat(packetToUser.data, data);
                strcat(packetToUser.data, " is Full.\n");
                packetToUser.size = strlen(packetToUser.data);
            }
            else if (res == 2){ 
                packetToUser.type = JN_NAK;
                char* temp;
                strcpy(packetToUser.data, "**JN_NAK**: Session: ");
                strcat(packetToUser.data, data);
                strcat(packetToUser.data, " does not exist.\n");
                packetToUser.size = strlen(packetToUser.data);
            }
            memset(&buf, 0, MAX_BUFF_SIZE);
            StrToPack(&packetToUser, buf);
            if (send(*socket, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "SERVER FUNCTION ERROR: sent client NS_ACK error. \n");
            }
        }
        if (packetRev.type == LEAVE_SESS) { 
            leave_session(id);
        }
        if (packetRev.type == NEW_SESS) {
            strncpy(data, (char *)(packetRev.data), MAX_DATA_SIZE);
            int res = create_session(data, id, socket);
            if (res == 0){
                packetToUser.type = NS_ACK; 
                packetToUser.size = 0;
            } 
            else if (res == 1){
                packetToUser.type = NS_NAK; 
                strcpy(packetToUser.data, "**NS_NAK**: You are in a session now \n");
                packetToUser.size = strlen(packetToUser.data);
            }
            else if (res == 3){ 
                packetToUser.type = NS_NAK;
                strcpy(packetToUser.data, "**NS_NAK**: reach maxmium session \n");
                packetToUser.size = strlen(packetToUser.data);
            }
            else if (res == 2){ 
                packetToUser.type = NS_NAK; 
                strcpy(packetToUser.data, "**NS_NAK**: Session name exists\n");
                packetToUser.size = strlen(packetToUser.data);
            }
            memset(&buf, 0, MAX_BUFF_SIZE);
            StrToPack(&packetToUser, buf);
            if (send(*socket, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "SERVER FUNCTION ERROR: sent client NS_ACK error. \n");
            }
        }
        if (packetRev.type == QUERY) { 
            packetToUser.type = QU_ACK;
            char* temp = serverlist();
            memset(&packetToUser.data, 0, packetToUser.size);
            strcpy(packetToUser.data, temp);
            packetToUser.size = strlen(packetToUser.data);
            memset(&buf, 0, MAX_BUFF_SIZE);
            StrToPack(&packetToUser, buf);
            if (send(*socket, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "SERVER FUNCTION ERROR: sent client QU_ACK error. \n");
            }
        }
        if (packetRev.type == MESSAGE) { 
            broadcast(packetRev.data, id);
        }
        if (packetRev.type == PRIVATE){
            strncpy(target_id, (char *)(packetRev.source), MAX_SOURCE_SIZE);
            fprintf(stdout,"The target id is: %s \n",target_id);  
            fprintf(stdout,"The id is: %s \n",id);  
            private_broadcast(packetRev.data, id, target_id);

        }
        if (packetRev.type == REGISTER) { 
            strncpy(id, (char *)(packetRev.source), MAX_SOURCE_SIZE);
            strncpy(data, (char *)(packetRev.data), MAX_DATA_SIZE);

            if (reg_user(id,data,"registerlist.txt") == 3){
                packetToUser.type = RG_ACK; 
                strcpy(packetToUser.source, id);
                packetToUser.size = 0;
            }
            else {
                packetToUser.type = RG_NAK; 
                strcpy(packetToUser.source, id);
                strcpy(packetToUser.data, "**REG_NAK** : User name exist, register decline\n");
                packetToUser.size = strlen(packetToUser.data);
            }

            memset(&buf, 0, MAX_BUFF_SIZE);
            StrToPack(&packetToUser, buf);
            send(*socket, buf, MAX_BUFF_SIZE, 0);
            close(*socket);
            }
        else
            fprintf(stdout, "SERVER FUNCTION ERROR \n");
    } 
}


int main(int argc, char* argv[]){
    if (argc != 2){
        printf("Input command error \n");
        return -1;
    }
    //setup variables
    bool connect = false;
    char* port_num = argv[1];
    int socketfd;
    
    struct addrinfo hint, *res;
    memset(&hint, 0, sizeof hint);
    hint.ai_family = AF_INET; // IPv4  
    hint.ai_socktype = SOCK_STREAM; // for TCP
    hint.ai_flags = AI_PASSIVE;     // auto fill ip

    //set all client's and session's default status
    for (int i = 0; i < MAX_USER_SIZE; i++){
        client_list[i].connection = false;
        client_list[i].sess_state = false;
    }
    for (int i = 0; i < MAX_SESSION; i++){
        session_list[i].num_of_user = 0;
    }
    if (getaddrinfo(NULL, port_num, &hint, &res) != 0){
        fprintf(stdout, "ERROR:can not connect to the port \n");
        return -1;
    }

    // find the avaiable connnect
    struct addrinfo *temp;
    for (temp = res; temp != NULL; temp = temp->ai_next) {
        socketfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (socketfd == -1) {
            perror("socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(socketfd);
            continue;
        }

        if (bind(socketfd, temp->ai_addr, temp->ai_addrlen) == -1) {
            perror("bind");
            close(socketfd);
            continue;
        }
        //connect success
        connect = true;
        break; 
    }
    
    //check TCP listening
    if (listen(socketfd, 10) ==-1){
        perror("listening server");
        close(socketfd);
        return -1;
    }
    
    // accept messages and create thread
    while (true) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        int* new_socket = malloc(sizeof(int)); 
        *new_socket = accept(socketfd, (struct sockaddr *)&addr, &len);
        if (*new_socket == -1) {
            fprintf(stdout, "ERROR: server cannot accept \n");
            continue;
        }
        pthread_t thread;
        int* thread_arg = malloc(sizeof(int));
        *thread_arg = *new_socket;
        if (pthread_create(&thread, NULL, server_func, (void*)thread_arg) != 0) {
            fprintf(stdout, "ERROR: Failed to create thread. \n");
            close(*new_socket);
            free(new_socket);
            free(thread_arg);
            continue;
        }
        pthread_detach(thread);
    }

    return 0;
}