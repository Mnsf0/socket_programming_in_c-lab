#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

void* check_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(void) {
    fd_set list, read_list;
    int max_set;

    int listener, client;
    struct sockaddr_storage client_addr;
    socklen_t addrlen;

    char buffer[256];
    int nbytes;

    int yes = 1;
    int rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&list);
    FD_ZERO(&read_list);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, "6767", &hints, &ai)) != 0) {
        fprintf(stderr, "select server: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "select server: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai);
    puts("Binding successful!");

    if (listen(listener, 10) == -1) {
        perror("[-]listener");
        exit(3);
    }

    puts("Server listening on port 6767...\n");

    FD_SET(listener, &list);
    max_set = listener;

    for (;;) {
        read_list = list;

        if (select(max_set + 1, &read_list, NULL, NULL, NULL) == -1) {
            perror("[-]select");
            exit(4);
        }

        for (int i = 0; i <= max_set; i++) {
            if (FD_ISSET(i, &read_list)) {

                if (i == listener) {
                    // Accept new client connection
                    addrlen = sizeof(client_addr);
                    client = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
                    if (client == -1) {
                        perror("[-]accept");
                    } else {
                        FD_SET(client, &list);
                        if (client > max_set) {
                            max_set = client;
                        }
                        printf("select server: new connection on socket %d\n", client);
                    }
                } else {
                    // Receive incoming client message
                    if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        if (nbytes == 0) {
                            printf("select server: socket %d hung up\n", i);
                        } else {
                            perror("[-]recv");
                        }
                        close(i);
                        FD_CLR(i, &list);
                    } else {
                        // Broadcast message to all other connected clients
                        for (int j = 0; j <= max_set; j++) {
                            if (FD_ISSET(j, &list)) {
                                if (j != listener && j != i) {
                                    if (send(j, buffer, nbytes, 0) == -1) {
                                        perror("[-]send");
                                    }
                                }
                            }
                        }
                    }
                }

            }
        }
    }

    return 0;
}
