#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#define bufsize 1000

struct packet{
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

void fill_packet(struct packet* p, const char* buf) {
  char* temp[4];
  int start = 0;
  int count = 0;
  for (int i = 0; i < 1000 - 1; i++) {
    if (buf[i] == ':') {
      int l = i - start;//length of each section
      temp[count] = (char*)malloc(sizeof(char) * l);
      memcpy(temp[count], buf + start, l);
      start = i + 1; // the char after ":"
      count++;
      if (count == 4) break; // there should only be 4 colons according to packet structure
    }
  }

  p->total_frag = atoi(temp[0]);
  p->frag_no = atoi(temp[1]);
  p->size = atoi(temp[2]);
  for (int i = 0; i < 3; i++) free(temp[i]); // free memory
  p->filename = temp[3];

  memcpy(&(p->filedata), buf + start, p->size);
}

int get_random(){
    return (int)rand() % 100;
}

int main(int argc, char* argv[]){
    if (argc != 2){
        printf("The input is wrong\n");
        exit(1);
    }


    int sockfd, bind_adre; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *res;
    //char* my_port = argv[1];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE;     // use my IP
    getaddrinfo(NULL, argv[1], &hints, &res);
    printf("going to make a socket\n");
    // make a socket, bind it:
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    //bind with port 
    bind_adre = bind(sockfd, res->ai_addr, res->ai_addrlen);
    printf("Start \n");

    if(sockfd < 0 || bind_adre == -1){
        printf("binding error, try other port number\n");
        exit(1);
    } 
    printf("Binding Finish\n");

    //receive messges
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size = sizeof(their_addr);
    char buffer[bufsize];
    //int  buf_len = bufsize -1;

    int recv_b = recvfrom(sockfd, buffer,sizeof(buffer),0,(struct sockaddr*)&their_addr,&sin_size);
    if(recv_b == -1){
        printf("error occurred, recieve failed\n");
    }

    //buffer[recv_b] = '\0';
    
    if(strcmp(buffer,"ftp") == 0){
        recv_b = sendto(sockfd, "yes",4,0,(struct sockaddr*)&their_addr,sizeof(their_addr));
        // if(recv_b == -1){
        //     printf("sent error\n");
        //     exit(1);
        //}
    }else{
        recv_b = sendto(sockfd, "no",3,0,(struct sockaddr*)&their_addr,sizeof(their_addr));
        // if(recv_b == -1){
        //     printf("sent error\n");
        //     exit(1);
        // }
    }

    FILE *fp = NULL;
    bool Done = true;
    while(Done){

        char* file = (char*)malloc(sizeof(char)*1100);
        //setup files fill with all 0 for inital
        memset(file,0,1100);
        
        int recv_b = recvfrom(sockfd, file ,1100,0,(struct sockaddr*)&their_addr,&sin_size);
        if(recv_b == -1){
        
            recv_b = sendto(sockfd, "NACK",5,0,(struct sockaddr*)&their_addr,sin_size);
            free(file);
            continue;
        }
        else{

            if (get_random()>30){

            
                recv_b = sendto(sockfd, "ACK",4,0,(struct sockaddr*)&their_addr,sin_size);
                printf("normal at here ack\n");   

                struct packet packets;
                printf("normal at here\n");
                fill_packet(&packets, file);
                free(file);
                
                if(packets.frag_no == 1){
                    fp = fopen(packets.filename, "w");
                }

                if(fp == NULL){
                    printf("Did not open file\n");
                    exit(1);
                }
                if(fwrite(packets.filedata, sizeof(char), packets.size, fp) != packets.size){
                    printf("Did not write file with packet %d\n", packets.frag_no);
                }

                printf("Transfered packet %d  , Total: %d packets \n", packets.frag_no, packets.total_frag);
                
                if(packets.frag_no == packets.total_frag){
                    printf("Last One: Transfered packet %d  , Total: %d packets \n", packets.frag_no, packets.total_frag);
                    Done = false;
                }
            }
            else{
                printf("Packet drops");
                sendto(sockfd, "Packet drops", 13, 0, (struct sockaddr*) &their_addr, sin_size);
                continue;
            }
        }
    }
    printf("break at here\n");
    fclose(fp);
}