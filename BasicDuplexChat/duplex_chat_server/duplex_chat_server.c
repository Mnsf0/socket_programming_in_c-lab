#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#define BACKLOG 25

int main(int argc, char** argv){

  int listen_socket, comminucation_sock;

  struct sockaddr_in listen_address, comminucation_address;

  //we need to make only the comminucation_sock 
  //as socklen_t because we gonna send it as pointer, while the listen_socket
  //is local on the same machine so it's better to use sizeof()
  //since socklen_t consider costy 
  socklen_t size_comminucation_sock = sizeof(comminucation_address);

  //making the buffer for both send and recive 
  char recvBuffer[256], sendBuffer[256];
  pid_t cpid;
   
  //make all the memory zeros 
  bzero(&listen_address,sizeof(listen_address));
  listen_address.sin_family = AF_INET;
  listen_address.sin_port = htons(15151);
  listen_address.sin_addr.s_addr = INADDR_ANY;

  listen_socket = socket(AF_INET, SOCK_STREAM,0);
  
  if(bind(listen_socket,(struct sockaddr*)& listen_address, sizeof(listen_address)) == -1){
    perror("[-]BIND\n");
    exit(EXIT_FAILURE);
  }

  if(listen(listen_socket,BACKLOG) == -1){
    perror("[-]LISTEN\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]LISTEN\n");

  comminucation_sock = accept(listen_socket,(struct sockaddr*)&comminucation_address,&size_comminucation_sock);

  if(comminucation_sock == -1){
    perror("[-]ACCEPT\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]ACCEPT\n");

 //fork child process to handle the client connectino
  cpid = fork();
  if(cpid == 0){ // child process
    while(1){
      //receive data from client
      bzero(&recvBuffer, sizeof(recvBuffer));
      if(recv(comminucation_sock,recvBuffer,sizeof(recvBuffer),0) <= 0){
        perror("[-]recive fallied\n");
        exit(EXIT_FAILURE);
      }
      printf("\nClient: %s\n",recvBuffer);
    }
  }else {// parent process
    while(1){
    // read message form the user
     bzero(&sendBuffer,sizeof(sendBuffer));
    printf("\nType message: \n");
    fgets(sendBuffer,256,stdin);

      //send meessage to the client
      send(comminucation_sock,sendBuffer,strlen(sendBuffer)+1,0);
      printf("Message sent! \n");
    }
  }
  


  return 0;
}
