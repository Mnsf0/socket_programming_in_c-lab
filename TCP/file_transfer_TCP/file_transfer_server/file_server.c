#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT "8787"

int main() {
    int sockfd = -1, client = -1;
    struct sockaddr_storage clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    int yes = 1;
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &ai) != 0) {
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }  

    freeaddrinfo(ai);

    if (p == NULL || sockfd < 0) {
        exit(2);
    }

    if (listen(sockfd, 10) == -1) {
        exit(3);
    }

    if ((client = accept(sockfd, (struct sockaddr*)&clientaddr, &addrlen)) == -1) {
        exit(EXIT_FAILURE);
    }

    char file_send[1024] = {0};
    ssize_t bytes_read = read(client, file_send, sizeof(file_send) - 1);
    if (bytes_read <= 0) {
        close(client);
        close(sockfd);
        return 1;
    }

    file_send[bytes_read] = '\0';
    file_send[strcspn(file_send, "\r\n")] = '\0';

    FILE* readfile = fopen(file_send, "rb");
    unsigned char file_buff[1024];

    if (readfile != NULL) {
        file_buff[0] = 'y';
        send(client, file_buff, 1, 0);  

        size_t size;
        while ((size = fread(file_buff, 1, sizeof(file_buff), readfile)) > 0) {
            send(client, file_buff, size, 0);
        }
        fclose(readfile);
    } else {
        file_buff[0] = 'n';
        send(client, file_buff, 1, 0);   
    }

    close(client);
    close(sockfd);

    return 0;
}
