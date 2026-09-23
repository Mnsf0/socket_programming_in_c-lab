#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>

static int check_port(const char* port){
  char* endptr;
  errno = 0;
  long val = strtol(port, &endptr,10);

  if(errno != 0 || *endptr != '\0' || endptr == port){
      fprintf(stderr, "[-]PORT\n");
      exit(1);
    }  
  if(val < 1 || val > 65535){
      fprintf(stderr, "[-]PORT\n");
      exit(1);
    }
  return (int)val;
 }

int setup_ipv4_socket(const char *ip, int port, const char *message) {
    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));

    addr4.sin_family = AF_INET;
    addr4.sin_port   = htons(port);

    if (inet_pton(AF_INET, ip, &addr4.sin_addr) != 1) {
        fprintf(stderr, "setup_ipv4_socket: invalid IPv4 address\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket (IPv4/UDP)");
        return -1;
    }

    ssize_t sent = sendto(sockfd, message, strlen(message), 0,
                           (struct sockaddr *)&addr4, sizeof(addr4));
    if (sent < 0) {
        perror("sendto (IPv4/UDP)");
        close(sockfd);
        return -1;
    }

    printf("Sent %zd bytes to %s:%d via IPv4\n", sent, ip, port);
    close(sockfd);
    return 0;
}

int setup_ipv6_socket(const char *ip, int port, const char *message) {
    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));

    addr6.sin6_family = AF_INET6;
    addr6.sin6_port   = htons(port);

    if (inet_pton(AF_INET6, ip, &addr6.sin6_addr) != 1) {
        fprintf(stderr, "setup_ipv6_socket: invalid IPv6 address\n");
        return -1;
    }

    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket (IPv6/UDP)");
        return -1;
    }

    ssize_t sent = sendto(sockfd, message, strlen(message), 0,
                           (struct sockaddr *)&addr6, sizeof(addr6));
    if (sent < 0) {
        perror("sendto (IPv6/UDP)");
        close(sockfd);
        return -1;
    }

    printf("Sent %zd bytes to %s:%d via IPv6\n", sent, ip, port);
    close(sockfd);
    return 0;
}

int create_socket_from_ip(const char *ip, int port, const char *message) {
    struct sockaddr_in  test4;
    struct sockaddr_in6 test6;

    if (inet_pton(AF_INET, ip, &test4.sin_addr) == 1) {
        return setup_ipv4_socket(ip, port, message);
    }
    else if (inet_pton(AF_INET6, ip, &test6.sin6_addr) == 1) {
        return setup_ipv6_socket(ip, port, message);
    }
    else {
        fprintf(stderr, "create_socket_from_ip: '%s' is not a valid IPv4 or IPv6 address\n", ip);
        return -1;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <destination IP> <PORT number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    #define PORT check_port(argv[2])

    const char *message = "Hello from UDP server!";

    if (create_socket_from_ip(argv[1], PORT, message) < 0) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
