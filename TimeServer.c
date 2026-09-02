#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 25

int main(int argc, char** argv){

  if(argc != 2){
    printf("Usaage: %s <port>\n",argv[0]);
    exit(EXIT_FAILURE);
  }

  int port = atoi(argv[1]);
  printf("[+]PORT %d\n",port);

  int number_of_clients = 0;
  int sockfd = socket(AF_INET, SOCK_STREAM,0);

  struct sockaddr_in socket_address;
  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(port);
  socket_address.sin_addr.s_addr = INADDR_ANY;

  socklen_t size = sizeof(socket_address);

   if(bind(sockfd, (struct sockaddr*) &socket_address,size) == -1){
    perror("[-]BIND\n");
    exit(EXIT_FAILURE);
  }
  
  if(getsockname(sockfd,(struct sockaddr*)&socket_address,&size) == 0){
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &socket_address.sin_addr,ip,sizeof(ip));
    printf("[+]BIND:%s\n",ip);
  }

  if(listen(sockfd,BACKLOG) == -1){
    perror("[-]LISTEN\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]LISTEN\n");

  while(1){
  int client_socket = accept(sockfd,NULL,NULL);
    printf("[+]ACCEPT\n");
    number_of_clients++;

    time_t currentTime;
    time(&currentTime);

    printf("Client:%d requested for tiume at %s",number_of_clients, ctime(&currentTime));

    if(send(client_socket,ctime(&current_time),30,0) == -1){
      perror("[-]SEND\n");
      exit(EXIT_FAILURE);
    }
    printf("[+]SEND\n");
  }

  close(sockfd);


  return 0;
}
