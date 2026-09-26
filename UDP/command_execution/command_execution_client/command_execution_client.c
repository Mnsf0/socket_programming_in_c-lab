#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define COMMSIZE 1024
#define BUFFSIZE 1024

static int check_port(const char *s) {
    // strtol() would also accept leading whitespace and +, - signs
    if (!isdigit((unsigned char)s[0]))
        return -1;

    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v < 1 || v > 65535)
        return -1;
    return (int)v;
}

static int check_IP_version(const char *ip, int port, struct sockaddr_storage *addr, socklen_t *len) {
    struct sockaddr_in  *addr4 = (struct sockaddr_in *)addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET, ip, &addr4->sin_addr) == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons((uint16_t)port);
        *len = sizeof(*addr4);
        return 0;
    }

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET6, ip, &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((uint16_t)port);
        *len = sizeof(*addr6);
        return 0;
    }

    return -1;
}

int main(int argc, char** argv) {

  if(argc != 5){
   fprintf(stderr, "Usage: %s <local IP address > <local PORT> <server IP addres> <server PORT number>\n",argv[0]);
    exit(EXIT_FAILURE);
  }
    int local_port = check_port(argv[2]);
    if (local_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }
    int server_port  = check_port(argv[4]);
    if (server_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage local_addr, server_addr;
    socklen_t local_addr_len, server_addr_len;

    if (check_IP_version(argv[1], local_port, &local_addr, &local_addr_len) < 0) {
        fprintf(stderr, "[-] Invalid local IP address\n");
        exit(EXIT_FAILURE);
    }
    if (check_IP_version(argv[3], server_port, &server_addr, &server_addr_len) < 0) {
        fprintf(stderr, "[-] Invalid server IP address\n");
        exit(EXIT_FAILURE);
    }
    if (server_addr.ss_family != local_addr.ss_family) {
        fprintf(stderr, "[-] Local IP and server IP must be the same version (both IPv4 or both IPv6)\n");
        exit(EXIT_FAILURE);
    }

    
    int sockfd = socket(local_addr.ss_family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] SOKCET");
        exit(EXIT_FAILURE);
    }
  int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt(SO_REUSEADDR)");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    #ifdef SO_REUSEPORT
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
          perror("[-] setsockopt(SO_REUSEPORT)");
          close(sockfd);
          exit(EXIT_FAILURE);
      }
    #endif

  if (bind(sockfd, (struct sockaddr *)&local_addr, local_addr_len) < 0) {
    perror("[-] BIND");
    close(sockfd);
    exit(EXIT_FAILURE);
}

  if (connect(sockfd, (struct sockaddr *)&server_addr, server_addr_len) < 0) {
        perror("[-] CONNECT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Client bound to %s:%d and connected to server %s:%d\n",
         argv[1], local_port, argv[3], server_port);

  while(1){
    char command[COMMSIZE];
    char response[BUFFSIZE];

    printf("$");
    fflush(stdout);
    
    if(fgets(command, BUFFSIZE, stdin) == NULL ) break ;
    
    command[strcspn(command, "\r\n")] = '\0';
    if(strcmp(command, "/quite") == 0 || strcmp(command, "/quite") == 0){
      printf("[+] Exiting\n");
    }else if(strlen(command) == 0) continue;


    ssize_t sent = send(sockfd, command, strlen(command), 0 );


    if(sent < 0) {
      fprintf(stderr, "[-]SEND\n");
      exit(EXIT_FAILURE);
    }

    
    ssize_t recved = recv(sockfd, response, BUFFSIZE-1,0);
    if(recv < 0){
      fprintf(stderr, "[-]recv\n");
      exit(EXIT_FAILURE);
    }

    response[recved] = '\0';
    printf("recv:\n%s\n", response);
  }

  close(sockfd);
  return 0;
}
