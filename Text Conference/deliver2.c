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
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "packet.h"


/*
 * 
 */


char* PacketToString(struct packet p, int *len) {
    char *buf = (char *)malloc(sizeof(char)*100);
    memset(buf, 0, sizeof(buf));
    int length = snprintf(buf, 100, "%d:%d:%d:%s:", p.total_frag, p.frag_no, p.size, p.filename);
    buf = (char *)realloc(buf, p.size + length);
    memcpy(buf + length, p.filedata, p.size);
    *len = length + p.size;
    return buf;
}

int main(int argc, char** argv) {
    printf("hello\n");
    if (argc != 3){
        fprintf(stdout, "Incorrect Format\n");
        exit(0);
    }
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1){
        fprintf(stderr, "socket invalid\n");
        exit(1);
    }
    
    int ip = atoi(argv[1]);
    int port = atoi(argv[2]);
    
    struct sockaddr_in addr_serv;
    memset(&addr_serv, 0, sizeof(struct sockaddr_in));
    addr_serv.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &(addr_serv.sin_addr));
    
    char user_input[999];
    char *name;
    
    printf("ftp <file name>\n");
    
    while (true){
        fgets(user_input, 999, stdin);
        if (user_input[0] == 'f' && user_input[1] == 't' && user_input[2] == 'p' && user_input[3] == ' '){
            //printf("hi\n");
            strtok(user_input, " ");
            name = strtok(NULL, " \n\t");

            if (access(name, F_OK) != -1) {
                printf("file: yes \n");
                break;
                // file exists
            } 
            else {
                printf("file does not exist, please try again\n");
                exit(1);
                // file doesn't exist
            }
            
        }
        else{
            printf("Incorrect Input, please try again\n");
        }
    }
    
    //struct timeval begin, end;
    clock_t send_start, now, begin, end;
    
    int byte_send;
    int byte_recv;
    char send_buf[] = "ftp";
    char recv_buf[999];
    
    begin = clock();

    
    byte_send = sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr*)&addr_serv, sizeof(addr_serv));
    
    if (byte_send == -1){
        perror("send error");
        exit(1);
    }
    
    socklen_t addr_size = sizeof(addr_serv);

    byte_recv = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&addr_serv, &addr_size);

    if (byte_recv == -1){
        perror("receive error");
        exit(1);
    }
    
    end = clock();
    
    double RTT = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("RTT is: %f μs\n", RTT*1000000);
    
    if (strcmp(recv_buf, "yes") == 0) {
        printf( "A file transfer can start\n");
    }
    
    //lab2
    
    FILE *fp = fopen(name, "r");
    if(fp == NULL){
        printf("File open error\n"); 
        exit(1);
    }
    
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    printf("Total size %d\n",file_size);
    fseek(fp, 0, SEEK_SET);
    
    int total_frag = file_size / 1000;
    int last_packet = file_size % 1000;
    if(last_packet != 0){
        total_frag+=1;
    }
    printf("Total packets %d, last packet size = %d\n",total_frag, last_packet);
    
    for (int i = 0; i < total_frag; i++){
        struct packet p;
        p.filename = name;
        //printf("name: %s\n", p.filename);
        p.frag_no = i+1;
        p.total_frag = total_frag;
        if (i == total_frag -1){
            p.size = last_packet;
        }
        else{
            p.size = 1000;
        }
        memset(p.filedata, 0, sizeof(p.filedata));
        fread(p.filedata, sizeof(char), p.size, fp);
        
        int len = 0;
        char *send_data = PacketToString(p, &len);
        
        begin = clock();
        
        byte_send = sendto(sockfd, send_data, len, 0, (struct sockaddr*)&addr_serv, sizeof(addr_serv));

        if (byte_send == -1){
            perror("send error");
            exit(1);
        }
//        else{
//            printf("Send Packets %d/%d\r", i, total_frag);
//        }
        bool timeout = false;
        
        struct timeval time_out;
        time_out.tv_sec=0;
        time_out.tv_usec = 1000;
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_out, sizeof(time_out))<0){
            perror("error");
        }
        
        while (!timeout){

            byte_recv = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&addr_serv, &addr_size);
            

            
            now = clock();
            double time_passed = ((double)(now - begin)) / CLOCKS_PER_SEC;
            printf("time: %f μs\n", time_passed *1000000);
            //printf("now 1: %f μs\n", now *1000000);
            //printf("now: %f μs\n", now);
            
            if (byte_recv < 0){
                printf("receive error\n");
                //perror("receive error");
                //exit(1);
            }
            
            else if (strcmp(recv_buf, "ACK") == 0){
                
                printf("Packet %d received successful!!\n", i);
                timeout = true;
                continue;
                
            }
            else if(strcmp(recv_buf, "ACK") != 0 || time_passed == RTT){
                printf("A packet did not receive.\n");
                
                now = clock();
                
                time_passed = ((double)(now - begin)) / CLOCKS_PER_SEC;
                //printf("time passed: %f μs\n", time_passed*1000000);
                //printf("now 2: %f μs\n", now *1000000);
                //if (time_passed >= 2*RTT){
                    //printf("time passed 2: %f μs\n", time_passed*1000000);
                    begin = clock();
                
                    byte_send = sendto(sockfd, send_data, len, 0, (struct sockaddr*)&addr_serv, sizeof(addr_serv));

                    if (byte_send == -1){
                        perror("send error");
                        exit(1);
                    }
                //} 
                
                
                //exit(1);
            }
        }
    } 
    
    close(sockfd);
    
    return 0;
}

