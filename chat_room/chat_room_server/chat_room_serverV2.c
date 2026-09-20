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

#define INITIAL_BUF_SIZE 256
#define MAX_MSG_SIZE     4096

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
        setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int));

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
                    // Receive incoming client message (as chunks)
                    size_t cap = INITIAL_BUF_SIZE;
                    size_t total = 0;
                    char *buf = malloc(cap); // using heap

                    if (buf == NULL) {
                        perror("[-]malloc failed\n");
                        close(i);
                        FD_CLR(i, &list);
                        continue;
                    }

                    int too_big = 0;
                    int hung_up = 0;
                    ssize_t n;
                    // MSG_DONTWAIT so the loop ends when no more data is ready
                    // instead of blocking the whole select() server
                    while ((n = recv(i, buf + total, cap - total, MSG_DONTWAIT)) > 0) {
                        // add received data to the total
                        total += n;

                        // buffer is full, need more room before next recv()
                        if (total == cap) {
                            // even with multiple chunks the message is too long
                            if (cap >= MAX_MSG_SIZE) {
                                printf("message exceeded max allowed size (%d bytes), aborting\n", MAX_MSG_SIZE);
                                too_big = 1;
                                break;
                            }

                            size_t new_cap = cap * 2;
                            if (new_cap > MAX_MSG_SIZE) {
                                new_cap = MAX_MSG_SIZE;
                            }

                            char *tmp = realloc(buf, new_cap);
                            if (tmp == NULL) {
                                printf("[-]realloc failed\n");
                                too_big = 1;
                                break;
                            }

                            buf = tmp;
                            cap = new_cap;
                        }
                    }

                    if (too_big) {
                        free(buf);
                        close(i);
                        FD_CLR(i, &list);
                        continue;
                    }

                    if (n == 0) {
                        hung_up = 1;
                    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("[-]recv failed\n");
                        free(buf);
                        close(i);
                        FD_CLR(i, &list);
                        continue;
                    }

                    // null terminate safely (make sure there's at least 1 spare byte)
                    if (total == cap) {
                        char *tmp = realloc(buf, cap + 1);
                        if (tmp == NULL) {
                            perror("realloc failed for null terminator\n");
                            free(buf);
                            close(i);
                            FD_CLR(i, &list);
                            continue;
                        }
                        buf = tmp;
                    }
                    buf[total] = '\0';

                    if (total > 0) {
                        print_sanitized(buf, total);

                        // Broadcast message to all other connected clients
                        for (int j = 0; j <= max_set; j++) {
                            if (FD_ISSET(j, &list)) {
                                if (j != listener && j != i) {
                                    if (send_all(j, buf, total) == -1) {
                                        perror("[-]send");
                                    }
                                }
                            }
                        }
                    }

                    free(buf);

                    if (hung_up) {
                        printf("select server: socket %d hung up\n", i);
                        close(i);
                        FD_CLR(i, &list);
                    }
                }

            }
        }
    }

    return 0;
}
