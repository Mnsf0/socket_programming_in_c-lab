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

static int send_all(int fd, const char *buf, size_t len){
  size_t sent = 0;
  while(sent < len){
    ssize_t n = send(fd, buf + sent, len - sent, 0);
    if(n == -1){
      if(errno == EINTR) continue;
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

static ssize_t recv_chunk(int fd, char *buf, size_t size){
  ssize_t n;
  do {
    n = recv(fd, buf, size, 0);
  } while(n == -1 && errno == EINTR);
  return n;
}

static void print_sanitized(const char *buf, size_t len){
  for(size_t i = 0; i < len; i++){
    unsigned char c = (unsigned char)buf[i];
    if(c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7f)){
      putchar(c);
    } else {
      printf("\\x%02x", c);
    }
  }
  putchar('\n');
}

int main(int argc, char** argv){

  int listen_socket, comminucation_sock;

  struct sockaddr_in6 listen_address, comminucation_address;

  socklen_t size_comminucation_sock = sizeof(comminucation_address);

  char recvBuffer[256], sendBuffer[256];
  pid_t cpid;

  bzero(&listen_address,sizeof(listen_address));
  listen_address.sin6_family = AF_INET6;
  listen_address.sin6_port = htons(15151);
  listen_address.sin6_addr = in6addr_any;

  listen_socket = socket(AF_INET6, SOCK_STREAM,0);
  if(listen_socket == -1){
    perror("[-]SOCKET");
    exit(EXIT_FAILURE);
  }

  int yes = 1;
  if(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
    perror("[-]SO_REUSEADDR");
    exit(EXIT_FAILURE);
  }

  int no = 0;
  if(setsockopt(listen_socket, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) == -1){
    perror("[-]IPV6_V6ONLY");
    exit(EXIT_FAILURE);
  }

  if(bind(listen_socket,(struct sockaddr*)& listen_address, sizeof(listen_address)) == -1){
    perror("[-]BIND");
    exit(EXIT_FAILURE);
  }

  if(listen(listen_socket,BACKLOG) == -1){
    perror("[-]LISTEN");
    exit(EXIT_FAILURE);
  }
  printf("[+]LISTEN\n");

  comminucation_sock = accept(listen_socket,(struct sockaddr*)&comminucation_address,&size_comminucation_sock);

  if(comminucation_sock == -1){
    perror("[-]ACCEPT");
    exit(EXIT_FAILURE);
  }
  printf("[+]ACCEPT\n");

  cpid = fork();
  if(cpid == 0){
    while(1){
      ssize_t n = recv_chunk(comminucation_sock, recvBuffer, sizeof(recvBuffer));
      if(n <= 0){
        perror("[-]recive fallied");
        exit(EXIT_FAILURE);
      }
      printf("\nClient: ");
      print_sanitized(recvBuffer, (size_t)n);
    }
  }else {
    while(1){
      bzero(&sendBuffer,sizeof(sendBuffer));
      printf("\nType message: \n");
      fgets(sendBuffer,256,stdin);

      send_all(comminucation_sock,sendBuffer,strlen(sendBuffer)+1);
      printf("Message sent! \n");
    }
  }

  return 0;
}
