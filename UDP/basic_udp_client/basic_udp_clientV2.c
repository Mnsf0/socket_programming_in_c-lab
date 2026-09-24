#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFFSIZE 1024
#define RECV_TIMEOUT_SEC 5

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

//check IP address 
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

static int print_sanitized(const char *data, size_t len) {
    int clean = 1; // assume everything is clear until otherwise proves 
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c >= 0x20 && c < 0x7f) {
            putchar(c);
        } else {
            printf("\\x%02x", c);
            clean = 0;
        }
    }
    return clean;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <local IP> <local PORT> <server IP> <server PORT>\n", argv[0]);
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

    struct timeval tv = { .tv_sec = RECV_TIMEOUT_SEC, .tv_usec = 0 };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("[-] SETSOCKOPT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    {
        char discard[BUFFSIZE];
        while (recv(sockfd, discard, sizeof(discard), MSG_DONTWAIT) >= 0)
            ;
    }

    const char msg[] = "hello";
    if (send(sockfd, msg, strlen(msg), 0) < 0) {
        perror("[-] SEND");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char recvbuff[BUFFSIZE + 1];
    ssize_t n = recv(sockfd, recvbuff, sizeof(recvbuff), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            fprintf(stderr, "[-] Timed out after %d \n", RECV_TIMEOUT_SEC);
        else
            perror("[-] RECV");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    size_t len = (size_t)n;
    int truncated = 0;
    if (len > BUFFSIZE) {
        len = BUFFSIZE;
        truncated = 1;
    }

    printf("[+] Received from server: ");
    int clean = print_sanitized(recvbuff, len);
    printf("\n");
    if (!clean)
        fprintf(stderr, "[!] WARNING: server data contained unsafe bytes (escaped above)\n");

    close(sockfd);
    return 0;
}
