#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define INITIAL_BUF_SIZE 512
#define MAX_MSG_SIZE 2048

int main() {

    int networkSocket;
    networkSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (networkSocket == -1) {
        perror("[-]sockets\n");
        return 1;
    }

    struct sockaddr_in ClientSocket;
    ClientSocket.sin_family = AF_INET;
    ClientSocket.sin_port = htons(7897);
    ClientSocket.sin_addr.s_addr = INADDR_ANY;

    int connectionStatu = connect(networkSocket, (struct sockaddr *) &ClientSocket, sizeof(ClientSocket));

    if (connectionStatu == -1) {
        perror("[-]connection\n");
        close(networkSocket);
        return 1;
    }

    
    size_t cap = INITIAL_BUF_SIZE;
    size_t total = 0;
    char *buf = malloc(cap); // using heap

    if (buf == NULL) {
        perror("[-]malloc failed\n");
        close(networkSocket);
        return 1;
    }

    ssize_t n;
    while ((n = recv(networkSocket, buf + total, cap - total, 0)) > 0) {
    // add received data to the total
        total += n;

        // buffer is full, need more room before next recv()
        if (total == cap) {
      //even with multiple chunks the message is too long 
            if (cap >= MAX_MSG_SIZE) {
                printf("message exceeded max allowed size (%d bytes), aborting\n", MAX_MSG_SIZE);
                free(buf);
                close(networkSocket);
                return 1;
            }

            size_t new_cap = cap * 2;
            if (new_cap > MAX_MSG_SIZE) {
                new_cap = MAX_MSG_SIZE;
            }

            char *tmp = realloc(buf, new_cap);
            if (tmp == NULL) {
                printf("[-]realloc failed\n");
                free(buf);
                close(networkSocket);
                return 1;
            }

            buf = tmp;
            cap = new_cap;
        }
    }

    if (n < 0) {
        perror("[-]recv failed\n");
        free(buf);
        close(networkSocket);
        return 1;
    }

    // n == 0 here means server closed the connection - loop ends naturally,

    // null terminate safely (we always leave room because we stop growing at MAX_MSG_SIZE,
    // but to be fully safe, we make sure there's at least 1 spare byte)
    if (total == cap) {
        char *tmp = realloc(buf, cap + 1);
        if (tmp == NULL) {
            perror("realloc failed for null terminator\n");
            free(buf);
            close(networkSocket);
            return 1;
        }
        buf = tmp;
    }
    buf[total] = '\0';

    printf("the message that come from the server is: %s\n", buf);
    printf("total bytes received: %zu\n", total);

    free(buf);
    close(networkSocket);

    return 0;
}
