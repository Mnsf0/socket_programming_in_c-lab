#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
  
  //check we got the coreect number of attrbuites 
  if(argc != 2){
    printf("Usage: %s <port>\n",argv[0]);
    exit(EXIT_FAILURE);
 }
  //assign the port given by the user to a variable
  int port = atoi(argv[1]);
  
  //make the buufer to recve time
  char buffr[30];
  //make the socket 
  int sockdf = socket(AF_INET, SOCK_STREAM,0);

  //make the address of the socket
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  //get the size of the address, 
  //I prefer this way rather then send sizeof() directly,
  //beauce I don't know the other system how it gonna handle size as interger
  //or with other data type 
  
  socklen_t size = sizeof(addr);

    if(connect(sockdf,(struct sockaddr*)&addr,size) == -1){
    perror("[-]connect\n");
    exit(EXIT_FAILURE);
  }

  printf("[+]connect\n");


  //recv tiem
  if(recv(sockdf, &buffr, sizeof(buffr),0) == -1){
    perror("[-]receve\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]receve\n");

  close(sockdf);

  return 0;

}
