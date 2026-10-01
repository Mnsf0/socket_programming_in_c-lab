#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFSIZE 1024

#define AUTH_TOKEN "ScioVSIOwQzXCBY26538"

typedef struct current_load {
   int current_user;
   double current_CPU_load;

}current_load;

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
int main(int argc, char** argv){
  if(argc != 3){
    fprintf(stderr, "[-]Usage: %s <IP address> <PORT number> \n",argv[0]);
    exit(EXIT_FAILURE);
  }

    int local_port = check_port(argv[2]);
    if (local_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }

     struct sockaddr_storage local_addr, remote_addr;
     socklen_t local_addr_len, remote_addr_len;

    if (check_IP_version(argv[1], local_port, &local_addr, &local_addr_len) < 0) {
       fprintf(stderr, "[-] Invalid local IP address\n");
       exit(EXIT_FAILURE);
    }

    current_load stats;
    memset(&stats, 0, sizeof(stats));

    int sockfd = socket(local_addr.ss_family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] SOKCET");
        exit(EXIT_FAILURE);
    }  

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 00000;


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



   int sent = sendto(sockfd, AUTH_TOKEN, strlen(AUTH_TOKEN),
                     0,(struct sockaddr*)& local_addr, local_addr_len);

  if(sent != strlen(AUTH_TOKEN)) {

    fprintf(stderr, "[-]SEND\n");
    exit(EXIT_FAILURE);
  }

  remote_addr_len = sizeof(remote_addr);
  int recv = recv = recvfrom(sockfd, &stats, sizeof(stats), 0, (struct sockaddr *)&remote_addr, &remote_addr_len);
if (recv < 0) {
    perror("[-]recv");
    exit(EXIT_FAILURE);
} else if (recv != sent) {
    perror("recv != sent");
    exit(EXIT_FAILURE);
}

printf("\tAvg. CPU load (1 min): %lf\n", stats.current_CPU_load);
printf("\tLogged in users: %d\n", stats.current_user);

  close(sockfd);
  return 0;
}
