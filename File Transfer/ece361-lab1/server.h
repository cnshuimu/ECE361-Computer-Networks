
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
#define LEN 1000


struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

void strToPacket(struct packet* p, const char* buf) {
  char* temp[4];
  int start = 0;
  int count = 0;
  for (int i = 0; i < LEN - 1; i++) {
    if (buf[i] == ':') {
      int l = i - start;//length of each section
      temp[count] = malloc(sizeof(char) * l);
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