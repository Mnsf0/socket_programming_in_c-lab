#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#define INITIAL_BUF_SIZE 64
#define MAX_MSG_SIZE 256
#define RECV_TIMEOUT_SEC 10

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    int sockdf = socket(AF_INET, SOCK_STREAM, 0);
    if (sockdf == -1) {
        perror("[-] socket creation failed");
        exit(2);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    socklen_t size = sizeof(addr);

    if (connect(sockdf, (struct sockaddr*)&addr, size) == -1) {
        perror("[-] connect failed");
        close(sockdf);
        exit(3);
    }

    printf("[+] connect\n");

    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(sockdf, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("[-] setsockopt failed");
        close(sockdf);
        exit(8);
    }

    size_t cap = INITIAL_BUF_SIZE;
    size_t total = 0;
    char *buf = malloc(cap);

    if (buf == NULL) {
        perror("[-] malloc failed");
        close(sockdf);
        exit(4);
    }

    ssize_t n;
    while ((n = recv(sockdf, buf + total, cap - total, 0)) > 0) {
        total += n;

        if (total == cap) {
            if (cap >= MAX_MSG_SIZE) {
                printf("[-] Message exceeded max size (%d bytes), aborting\n", MAX_MSG_SIZE);
                free(buf);
                close(sockdf);
                exit(5);
            }

            size_t new_cap = cap * 2;
            if (new_cap > MAX_MSG_SIZE) {
                new_cap = MAX_MSG_SIZE;
            }

            char *tmp = realloc(buf, new_cap);
            if (tmp == NULL) {
                perror("[-] realloc failed");
                free(buf);
                close(sockdf);
                exit(5);
            }

            buf = tmp;
            cap = new_cap;
        }
    }

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("[-] recv timed out after %d seconds — server took too long to respond\n", RECV_TIMEOUT_SEC);
        } else {
            perror("[-] recv failed");
        }
        free(buf);
        close(sockdf);
        exit(6);
    }

    if (total == cap) {
        char *tmp = realloc(buf, cap + 1);
        if (tmp == NULL) {
            perror("[-] realloc failed for null terminator");
            free(buf);
            close(sockdf);
            exit(7);
        }
        buf = tmp;
    }

    buf[total] = '\0';
    printf("[+] received (%zu bytes):\n%s\n", total, buf);

    free(buf);
    close(sockdf);

    return 0;
}
