#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <sys/time.h>
#include <signal.h>
#include <ctype.h>

#define RECV_TIMEOUT_SEC 10
#define INITIAL_BUF_SIZE 256
#define MAX_MSG_SIZE 4096

/*
the purpose of this function is to check every character before printing it on the screen,
but how? It checks three things before deciding if it's a good char or not:
1- is it \n? 2-is it \t?, 3-is it a char between the ASCII code of space and ~,
otherwise that's a bad character, for example, 
the character ESC if it's sent as one character, 
it's bad since it doesn't satisfy any of the conditions above,
but does the program remove it? no it just replaces the character with its 
ASCII code so at the end ESC = \x1b.
 */
static void print_sanitized(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7f)) {
            putchar(c);
        } else {
            printf("\\x%02x", c);
        }
    }
    putchar('\n');
}

static int parse_port(const char *arg) {
    char *endptr;
    errno = 0;
    long val = strtol(arg, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || endptr == arg) {
        fprintf(stderr, "[-] Invalid port: '%s' is not a number\n", arg);
        return -1;
    }
    if (val < 1 || val > 65535) {
        fprintf(stderr, "[-] Invalid port: %ld (must be 1-65535)\n", val);
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

static int connect_to(const char *host, const char *port_str) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai_err = getaddrinfo(host, port_str, &hints, &res);
    if (gai_err != 0) {
        fprintf(stderr, "[-] getaddrinfo: %s\n", gai_strerror(gai_err));
        return -1;
    }

    int sockfd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct timeval tv = { .tv_sec = RECV_TIMEOUT_SEC, .tv_usec = 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd == -1) {
        fprintf(stderr, "[-] Could not connect to %s:%s: %s\n", host, port_str, strerror(errno));
    }
    return sockfd;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (parse_port(argv[2]) == -1) {
        exit(EXIT_FAILURE);
    }

    int sockfd = connect_to(argv[1], argv[2]);
    if (sockfd == -1) {
        exit(EXIT_FAILURE);
    }
    printf("[+] Connected to %s:%s\n", argv[1], argv[2]);

    pid_t cpid = fork();
    if (cpid < 0) {
        perror("[-] fork");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {
        while (1) {
            size_t cap = INITIAL_BUF_SIZE;
            size_t total = 0;
            char *buf = malloc(cap);

            if (buf == NULL) {
                perror("[-] malloc failed");
                close(sockfd);
                exit(4);
            }

            ssize_t n;
            while ((n = recv(sockfd, buf + total, cap - total, 0)) > 0) {
                total += (size_t)n;

                if (total == cap) {
                    if (cap >= MAX_MSG_SIZE) {
                        printf("[-] Message exceeded max size (%d bytes), aborting\n", MAX_MSG_SIZE);
                        free(buf);
                        close(sockfd);
                        exit(5);
                    }
                    size_t new_cap = cap * 2;
                    if (new_cap > MAX_MSG_SIZE) new_cap = MAX_MSG_SIZE;

                    char *tmp = realloc(buf, new_cap);
                    if (tmp == NULL) {
                        perror("[-] realloc failed");
                        free(buf);
                        close(sockfd);
                        exit(5);
                    }
                    buf = tmp;
                    cap = new_cap;
                }
            }

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("[-] recv timed out after %d seconds\n", RECV_TIMEOUT_SEC);
                } else {
                    perror("[-] recv failed");
                }
                free(buf);
                close(sockfd);
                exit(6);
            }

              /*this is the new part in recv(), see in my previous code there was a bug 
               * that if no data arrives and the other side closes the connection
               * the recv() part gonna close too, but the probelm gonna be on the send, 
               * it won't close, and the connection will remain open, 
               * how is that? By retrieving the parent PID and sending a kill signal to it
               */
            if (n == 0 && total == 0) {
                printf("[-] Server disconnected\n");
                free(buf);
                close(sockfd);
                kill(getppid(), SIGUSR1);
                exit(0);
            }

            printf("[+] received (%zu bytes):\n", total);
            print_sanitized(buf, total);
            free(buf);
        }
    } else {
        char send_buffer[256];
        while (1) {
            memset(send_buffer, 0, sizeof(send_buffer));
            printf("Type Message: ");
            fflush(stdout);
            if (fgets(send_buffer, sizeof(send_buffer), stdin) == NULL) {
                break;
            }

            size_t msg_len = strlen(send_buffer);
            if (send_all(sockfd, send_buffer, msg_len) == -1) {
                perror("[-] send failed");
                break;
            }
            printf("[+] sent\n");
        }

        kill(cpid, SIGTERM);
    }

    close(sockfd);
    return 0;
}
