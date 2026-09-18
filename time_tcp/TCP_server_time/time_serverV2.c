#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 25


static int parse_port(const char *arg) {
    char *endptr;
    errno = 0;
    long val = strtol(arg, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || endptr == arg) {
        fprintf(stderr, "[-]Invalid port: '%s' is not a number\n", arg);
        return -1;
    }
    if (val < 1 || val > 65535) {
        fprintf(stderr, "[-]Invalid port: %ld (must be 1-65535)\n", val);
        return -1;
    }
    return (int)val;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = parse_port(argv[1]);
    if (port == -1) {
        exit(1);
    }
    printf("[+]PORT %d\n", port);

    signal(SIGPIPE, SIG_IGN);

    int number_of_clients = 0;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("[-]SOCKET");
        exit(2);
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("[-]SETSOCKOPT");
        close(sockfd);
        exit(3);
    }

    struct sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons((uint16_t)port);
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    socklen_t size = sizeof(socket_address);

    if (bind(sockfd, (struct sockaddr*)&socket_address, size) == -1) {
        perror("[-]BIND");
        close(sockfd);
        exit(4);
    }

    if (getsockname(sockfd, (struct sockaddr*)&socket_address, &size) == 0) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &socket_address.sin_addr, ip, sizeof(ip));
        printf("[+]BIND:%s\n", ip);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("[-]LISTEN");
        close(sockfd);
        exit(5);
    }
    printf("[+]LISTEN\n");

    while (1) {
        int client_socket = accept(sockfd, NULL, NULL);
        if (client_socket == -1) {
            if (errno == EINTR) continue;
            perror("[-]ACCEPT");
            continue;
        }
        printf("[+]ACCEPT\n");
        number_of_clients++;

        time_t currentTime;
        time(&currentTime);

       
        char time_buf[64];
        const char *ct = ctime(&currentTime);
        if (ct == NULL) {
            snprintf(time_buf, sizeof(time_buf), "unknown time\n");
        } else {
            snprintf(time_buf, sizeof(time_buf), "%s", ct);
        }
        size_t time_len = strlen(time_buf);

        printf("Client:%d requested for time at %s", number_of_clients, time_buf);

        if (send_all(client_socket, time_buf, time_len) == -1) {
            perror("[-]SEND");
            close(client_socket);
            continue;
        }
        printf("[+]SEND\n");

        close(client_socket);
    }

    close(sockfd);

    return 0;
}
