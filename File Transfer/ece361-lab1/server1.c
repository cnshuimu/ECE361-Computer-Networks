 #include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include "server.h"
#include <time.h>
#define SIZE 100
#define TOT 1100

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Too few/many arguments!\n");
        exit(1);
    }

    // created a socket
    // // created and initialized a socker address structure(Method 1 I chose to use the get adderinfo at the end)
    // struct sockaddr_in addr;
    // addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // addr.sin_port = htons(atoi(argv[1]));
    // addr.sin_family = AF_INET;
    // memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

    struct addrinfo hints;
    struct addrinfo *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // for either ipv4/ipv6
    hints.ai_socktype = SOCK_DGRAM; // for udp connection
    hints.ai_flags = AI_PASSIVE;
    int get = getaddrinfo(NULL, argv[1], &hints, &res);
    if(get != 0){
        printf("Did not get addrinfo, error code: %d", get);
    }

    int socketFD = socket(res->ai_family, res->ai_socktype, 0);
    // bind the socket with specific port number
    printf("Starting binding...\n");
    int bindyes = bind(socketFD, res->ai_addr, res->ai_addrlen);
    if(bindyes == -1){
        printf("Given port number is currently being used.\n");
        printf("Try other port number!\n");
        exit(1);
    }else{
        printf("Binding successfully!\n");
    }
    
    // start receiving message
    struct sockaddr_storage senderAdd;
    char receiveBuff[SIZE];
    socklen_t length = sizeof(senderAdd);
    printf("Waiting to receive message :)\n");
    int recB = recvfrom(socketFD, receiveBuff, sizeof(receiveBuff), 0, (struct sockaddr*) &senderAdd, &length);
    if(recB == -1){
        printf("Receiving failed\n");
        exit(1);
    }

    // check
    char yes[] = "yes";
    char no[] = "no";
    if(strcmp(receiveBuff, "ftp") == 0){
        printf("Received message!\n");
        printf("Sending ok back.\n");
        sendto(socketFD, yes, sizeof(yes), 0, (struct sockaddr*) &senderAdd, sizeof(senderAdd));
    }else{
        printf("Didn't receive.\n");
        printf("Sending no back\n");
        sendto(socketFD, no, sizeof(no), 0, (struct sockaddr*) &senderAdd, sizeof(senderAdd)); 
    }

    FILE *fptr = NULL;
    bool Done = false;
    char ack[] = "ACK";
    char nack[] = "NACK";
    char drop[] = "packet drops";
    srand(time(NULL)); // randomize seed
    double get_random() { return (double)rand() / (double)RAND_MAX; }
    

    while(!Done){
        char* files = (char *)malloc(sizeof(char) * TOT);
        memset(files, 0, sizeof(TOT));
        int recB = recvfrom(socketFD, files, TOT, 0, (struct sockaddr*) &senderAdd, &length);
        if(recB == -1){
            printf("NACK\n");
            sendto(socketFD, nack, sizeof(nack), 0, (struct sockaddr*) &senderAdd, length);
            free(files); 
            continue;
        }
        else{
            if (get_random() > 0.5) {
                printf("ACK\r");
                sendto(socketFD, ack, sizeof(ack), 0, (struct sockaddr*) &senderAdd, length);
                struct packet packets;
                strToPacket(&packets, files);
                free(files);
                if(packets.frag_no == 1){
                   fptr = fopen(packets.filename, "w");
                }
                if(fptr == NULL){
                   printf("Did not open file\n");
                   exit(1);
                }
                if(fwrite(packets.filedata, sizeof(char), packets.size, fptr) != packets.size){
                   printf("Did not write file with packet %d\n", packets.frag_no);
                }
                 //  printf("Transfered %d/%d the packets!\r", packets.frag_no, packets.total_frag);
        
               if(packets.frag_no == packets.total_frag){
                   printf("Transfered %d/%d the packets!\n", packets.frag_no, packets.total_frag);
                   Done = true;
                }
            }
            else{
                printf("Packet drops");
                sendto(socketFD, drop, sizeof(drop), 0, (struct sockaddr*) &senderAdd, length);
                continue;
           }
        }
       

   
    }
    fclose(fptr);
}