#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT "6767"
#define MAXDATASIZE 256
#define MAXNAMESIZE 32

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    char message[MAXDATASIZE];
    char nickName[MAXNAMESIZE];
    int sockfd;
    char sBuf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                            p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);

    puts("Nickname:");
    memset(nickName, 0, sizeof(nickName));
    memset(message, 0, sizeof(message));
    fgets(nickName, MAXNAMESIZE, stdin);

    puts("Connected\n");
    puts("[Type '/quit' to quit]");

    fd_set list, read_list;
    FD_ZERO(&list);
    FD_SET(sockfd, &list);
    FD_SET(STDIN_FILENO, &list);

    int max_list;

    if (sockfd > STDIN_FILENO) {
           max_list = sockfd;
    } else {
           max_list = STDIN_FILENO;
    }

    for (;;) {
        read_list = list;

        if (select(max_list + 1, &read_list, NULL, NULL, NULL) == -1) {
            perror("[-]select");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        char buffer[MAXDATASIZE];
        int nbytes;

        for (int i = 0; i <= max_list; i++) {
            if (FD_ISSET(i, &read_list)) {

                if (i == sockfd) {
                    if ((nbytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) <= 0) {
                        if (nbytes == 0) {
                            puts("\nServer disconnected.");
                        } else {
                            perror("[-]recv");
                        }
                        close(sockfd);
                        exit(EXIT_FAILURE);
                    } else {
                        buffer[nbytes] = '\0';
                        printf("%s", buffer);
                    }
                } else if (i == STDIN_FILENO) {
                    memset(sBuf, 0, sizeof(sBuf));
                    if (fgets(sBuf, sizeof(sBuf), stdin) == NULL) {
                        break;
                    }

                    if (strncmp(sBuf, "/quit", 5) == 0) {
                        close(sockfd);
                        return 0;
                    }

                    int count = 0;
                    while (count < strlen(nickName)) {
                        message[count] = nickName[count];
                        count++;
                    }

                    count--;
                    message[count] = ':';
                    count++;

                    for (int j = 0; j < strlen(sBuf); j++) {
                        message[count] = sBuf[j];
                        count++;
                    }
                    message[count] = '\0';

                    if (send(sockfd, message, strlen(message), 0) < 0) {
                        puts("Send failed");
                        close(sockfd);
                        return 1;
                    }

                    memset(sBuf, 0, sizeof(sBuf));
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
