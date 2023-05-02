#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include "helper.h"

bool in_session = false;
bool connected = false;

void *stub(void * arg){
    char buffer[MAX_BUFF_SIZE]; 
    memset(buffer, 0, MAX_BUFF_SIZE);
    
    int *socketfd = arg;
    
    while (true){
        packet pack;
        memset(buffer, 0, MAX_BUFF_SIZE);
        int recv_num = recv(*socketfd, buffer, MAX_BUFF_SIZE, 0);
        if (recv_num == -1){
            fprintf(stdout, "Server: stub receive error\n");
            return NULL;
        }
        if (recv_num == 0){
            continue;
        }

        PackToStr(&pack, buffer);
        
        if (pack.type == JN_ACK){ 
            fprintf(stdout, "Join_Session Succesfully.\n");
            in_session = true;
        }
        else if (pack.type == JN_NAK){
            fprintf(stdout, pack.data);
        }
        else if (pack.type == NS_ACK){
            fprintf(stdout, "Create_Session Successfully! \n");
        }
        else if (pack.type == NS_NAK){ 
            fprintf(stdout, pack.data);
        }
        else if (pack.type == MESSAGE){
            fprintf(stdout, pack.data);
        }
        else if (pack.type == QU_ACK){ 
            fprintf(stdout, pack.data);
        }
        else if (pack.type == 18){ // TIMEOUT
            fprintf(stdout, pack.data);
            in_session = false;
            connected = false;
            close(*socketfd);
            break;
        }
        else 
            fprintf(stdout, "Unexpected Type\n");
          
//        switch (pack.type){
//        case JN_ACK:
//            in_session = true;
//            fprintf(stdout, pack.data);
//        case JN_NAK:
//            in_session = false;
//            fprintf(stdout, pack.data);
//        case NS_ACK:
//            in_session = true;
//            fprintf(stdout, pack.data);
//        case QU_ACK:
//            fprintf(stdout, pack.data);
//        case MESSAGE:
//            fprintf(stdout, pack.data);
//        default:
//            fprintf(stdout, "Unexpected Type\n");
//        }   
    }
}

void registration(int *socketfd){
    /* Tokenize Input and Read Info */
    char *id = strtok(NULL, " ");
    char *password = strtok(NULL, " ");
    char *ip = strtok(NULL, " ");
    char *port = strtok(NULL, " ");
    
    if (id == NULL || password == NULL || ip == NULL || port == NULL || strtok(NULL, " ") != NULL){
        fprintf(stdout, "Server: Incorrect login format\n");
        return;
    }

    struct addrinfo addr_serv, *serv, *cur_serv ;
    memset(&addr_serv, 0, sizeof(addr_serv));
    addr_serv.ai_family = AF_INET; // IPv4  
    addr_serv.ai_socktype = SOCK_STREAM; 

    if (getaddrinfo(ip, port, &addr_serv, &serv) != 0){
        fprintf(stdout, "Server: get address failed\n");
        return;
    }

    for(cur_serv = serv; cur_serv != NULL; cur_serv = cur_serv->ai_next){
        *socketfd = socket(cur_serv->ai_family, cur_serv->ai_socktype, cur_serv->ai_protocol);
        if (*socketfd == -1) {
            fprintf(stdout, "Server: socket create failed\n");
            continue;
        }
        if (connect(*socketfd, cur_serv->ai_addr, cur_serv->ai_addrlen) == -1) {
            close(*socketfd);
            fprintf(stdout, "Server: connection failed\n");
            continue;
        }
    }

    packet pack;
    pack.type = REGISTER;
    strncpy(pack.source, id, MAX_SOURCE_SIZE);
    strncpy(pack.data, password, MAX_DATA_SIZE);
    pack.size = strlen(pack.data);
    
    
    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    StrToPack(&pack, buffer);
    if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: registration send error");
        close(*socketfd);
        return;
    }
    memset(buffer, 0, sizeof(buffer));
    if (recv(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: registration receive error \n");
        close(*socketfd);
        return;
    }

    PackToStr(&pack, buffer);
    if (pack.type == RG_ACK){
        fprintf(stdout, "Register Success.\n");
    }
    else if (pack.type == RG_NAK){
        fprintf(stdout, "Register Failed\n");
        fprintf(stdout, pack.data);
        close(*socketfd);
        return;
    }
    else {
        fprintf(stdout, "Register Error\n");
        close(*socketfd);
        return;
    }
    
}

void login(int *socketfd, pthread_t *thread){
    if (connected){
        fprintf(stdout, "Server: You have already logged in\n");
        return;
    }

    char *id = strtok(NULL, " ");
    char *password = strtok(NULL, " ");
    char *ip = strtok(NULL, " ");
    char *port = strtok(NULL, " ");

    if (id == NULL || password == NULL || ip == NULL || port == NULL || strtok(NULL, " ") != NULL){
        fprintf(stdout, "Server: Incorrect login format\n");
        return;
    }

    struct addrinfo addr_serv, *serv, *cur_serv ;
    memset(&addr_serv, 0, sizeof(addr_serv));
    addr_serv.ai_family = AF_INET; // IPv4  
    addr_serv.ai_socktype = SOCK_STREAM; // for TCP

    if (getaddrinfo(ip, port, &addr_serv, &serv) != 0){
        fprintf(stdout, "Server: get address failed\n");
        return;
    }

    for(cur_serv = serv; cur_serv != NULL; cur_serv = cur_serv->ai_next){
        *socketfd = socket(cur_serv->ai_family, cur_serv->ai_socktype, cur_serv->ai_protocol);
        if (*socketfd == -1) {
            fprintf(stdout, "Server: socket create failed\n");
            continue;
        }
        if (connect(*socketfd, cur_serv->ai_addr, cur_serv->ai_addrlen) == -1) {
            close(*socketfd);
            fprintf(stdout, "Server: connection failed\n");
            continue;
        }
        connected = true;
        break; 
    }

    if (cur_serv == NULL){
        fprintf(stdout, "Failed to connect to server\n");
        return;
    }

    packet pack;
    pack.type = LOGIN;
    strncpy(pack.source, id, MAX_SOURCE_SIZE);
    strncpy(pack.data, password, MAX_DATA_SIZE);
    pack.size = strlen(pack.data);

    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    StrToPack(&pack, buffer);
    if (send(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: login send error");
        close(*socketfd);
        return;
    }
    memset(buffer, 0, sizeof(buffer));
    if (recv(*socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: login receive error \n");
        close(*socketfd);
        return;
    }
    
    PackToStr(&pack, buffer);
    if (pack.type == LO_ACK){
        if (pthread_create(thread, NULL, stub, (void *)socketfd) == 0)
        fprintf(stdout, "Login Success.\n");
    }
    else if (pack.type == LO_NAK){
        fprintf(stdout, "Login Failed\n");
        fprintf(stdout, pack.data);
        connected = false;
        close(*socketfd);
        return;
    }
    else {
        fprintf(stdout, "Login Error \n");
        connected = false;
        close(*socketfd);
        return;
    }
}

void logout (int socketfd, pthread_t* thread){
    if (connected == false){
        fprintf(stdout, "No login detected\n");
        return;
    }
    packet pack;
  
    pack.type = EXIT;
    pack.size = 0;

    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, MAX_BUFF_SIZE);
    StrToPack(&pack, buffer);

    if (send(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: logout send error\n");
        return;
    }
    if (pthread_cancel(*thread) != 0){
        fprintf(stdout, "Server: logout thread cancel error \n");
    }
    
    connected = false;
    fprintf(stdout, "Logout Success\n");
    close(socketfd);
}

void createsession(int socketfd){
    if (!connected){
        fprintf(stdout, "No login detected\n");
        return;
    }
    
    char *id = strtok(NULL, " ");
    
    if (id == NULL){
        fprintf(stdout, "Server: /createsession <name>\n");
        return;
    }
 
    packet pack_to, pack_from;
    pack_to.type = NEW_SESS;
    strcpy(pack_to.data, id);
    pack_to.size = strlen(pack_to.data);
    
    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, MAX_BUFF_SIZE);
    
    StrToPack(&pack_to, buffer);
    if (send(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: create session send error\n");
        return;
    }
    memset(buffer, 0, sizeof(buffer));
    if (recv(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: create session receive error\n");
        return;
    }
    
    PackToStr(&pack_from, buffer);
    if (pack_from.type == NS_ACK){
        in_session = true;
        fprintf(stdout, "Create_Session Succesfully.\n");
    }
    else if (pack_from.type == NS_NAK){
        fprintf(stdout, pack_from.data);
    }
    else{
        fprintf(stdout, "Server: session create failed\n");
    }
}

void joinsession(int socketfd){
    if (!connected){
        fprintf(stdout, "No login detected\n");
        return;
    }
    if (in_session){
        fprintf(stdout, "Already in session, please leave to join a new one\n");
        return;
    }
    char *id = strtok(NULL, " ");

    if (id == NULL){
        fprintf(stdout, "Server: /joinsession <name>\n");
        return;
    }
    packet pack;
    pack.type = JOIN;
    strcpy(pack.data, id);
    pack.size = strlen(pack.data);

    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, MAX_BUFF_SIZE);
    StrToPack(&pack, buffer);

    if (send(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: join session send error\n");
        return;
    }
}

void leavesession (int socketfd){
    if (!connected){
        fprintf(stdout, "No login detected\n");
        return;
    }
    if (in_session == false){
        fprintf(stdout, "Server: Not in any session\n");
        return;
    }

    packet pack;
    pack.type = LEAVE_SESS;
    pack.size = 0;

    char buf[MAX_BUFF_SIZE];
    memset(buf, 0, MAX_BUFF_SIZE);
    
    StrToPack(&pack, buf);
    if (send(socketfd, buf, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: leaves session send error \n");
        return;
    }
    in_session = false;
    fprintf(stdout, "leave session success\n");
}

void list(int socketfd){
    if (!connected){
        fprintf(stdout, "No login detected\n");
        return;
    }
    packet pack, recv_pkt;
    pack.type = QUERY; 
    pack.size = 0;

    char buffer[MAX_BUFF_SIZE];
    memset(buffer, 0, MAX_BUFF_SIZE);
    
    StrToPack(&pack, buffer);

    if (send(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
        fprintf(stdout, "Server: list query error\n");
        return;
    }
}

int main(int argc, char *argv[]) {
    
    if (argc != 1){
        printf("Incorret Format, expect [./client]\n");
        return -1;
    }
    
    char buffer[MAX_BUFF_SIZE];
    char cmd[MAX_BUFF_SIZE];
    char *token;

    int socketfd;
    pthread_t thread;
    
    printf("Welcome!\n");
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        memset(cmd, 0, sizeof(buffer));

        fgets(buffer, MAX_BUFF_SIZE - 1, stdin);
        strcpy(cmd, buffer);
        cmd[strcspn(cmd, "\n")] = 0;
        
        token = buffer;
        while (*token == ' '){
            token++;
        }
        if (*token == '\n'){
            continue;
        }

        token = strtok(cmd, " ");
        
        if (strcmp(token, "/login") == 0){
            login(&socketfd, &thread);
        }
        else if (strcmp(token, "/logout") == 0){
            logout(socketfd, &thread);
        }
        else if (strcmp(token, "/createsession") == 0){
            createsession(socketfd);
        }
        else if (strcmp(token, "/leavesession") == 0){
            leavesession(socketfd);
        }
        else if (strcmp(token, "/joinsession") == 0){
            joinsession(socketfd);
        }
        else if (strcmp(token, "/list") == 0){
            list(socketfd);
        }
        else if (strcmp(token, "/quit") == 0){
            if (connected){
                logout(socketfd, &thread);
            }
            fprintf(stdout, "Connection End\n");
            break;
        }
        else if (strcmp(token, "/register") == 0){
            registration(&socketfd);
        }
        else if (strcmp(token, "/private") == 0){
            if (!connected){
                fprintf(stdout, "Server: Please login to the system\n");
                continue;
            }
            
            char *id = strtok(NULL, " ");
            char *data = strtok(NULL, "\n");
            
            fprintf(stdout, "id: %s, data: %s\n",id, data);
            
            if (id == NULL || data == NULL){
                fprintf(stdout, "Server: incorrect private message format\n");
                continue;
            }

            packet pack_to;
            pack_to.type = PRIVATE;
            strncpy(pack_to.source, id, MAX_SOURCE_SIZE);
            strncpy(pack_to.data, data, MAX_DATA_SIZE);
            pack_to.size = strlen(pack_to.data);

            char buffer[MAX_BUFF_SIZE];
            memset(buffer, 0, MAX_BUFF_SIZE);

            StrToPack(&pack_to, buffer);
            fprintf(stdout, "buffer: %s\n",buffer);
            if (send(socketfd, buffer, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "Server: private message send error\n");
            }   
        }
        else{
            if (!connected){
                fprintf(stdout, "Server: Please login to the system\n");
                continue;
            }
            if (!in_session){
                fprintf(stdout, "Server: Please join a session before sending messages\n");
                continue;
            }
            
            packet pack;
            pack.type = MESSAGE; 
            strcpy(pack.data, buffer);
            pack.size = strlen(pack.data);

            char buf[MAX_BUFF_SIZE];
            memset(buf, 0, MAX_BUFF_SIZE);
            StrToPack(&pack, buf);

            if (send(socketfd, buf, MAX_BUFF_SIZE, 0) == -1){
                fprintf(stdout, "Send Error \n");
            }
        }
    }
    return 0;
}
