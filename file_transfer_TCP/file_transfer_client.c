#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT "8787"

void* get_IP(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
    struct addrinfo hints, *ai, *p;
    int sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("127.0.0.1", PORT, &hints, &ai) != 0) {
        fprintf(stderr, "[-] Failed to resolve address\n");
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            sockfd = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(ai);

    if (p == NULL || sockfd < 0) {
        fprintf(stderr, "[-] Connection failed\n");
        exit(2);
    }
    printf("[+] Connected to server\n");
  
    char filename[1024] = {0};
    printf("Enter the file name: ");
    if (scanf("%1023s", filename) != 1) {
        close(sockfd);
        return 1;
    }

    if (send(sockfd, filename, strlen(filename), 0) == -1) {
        perror("[-] Send failed");
        exit(3);
    }
    printf("[+] Request sent\n");

    char byte = 0;
    ssize_t status_read = read(sockfd, &byte, 1);
    if (status_read <= 0) {
        perror("[-] Failed to read server response");
        exit(4);
    }

    if (byte == 'y') { 
        printf("File exists!\nStart downloading...\n");

        FILE* file = fopen(filename, "wb");
        if (!file) {
            perror("[-] Failed to create output file");
            close(sockfd);
            return 1;
        }

        char buffer[1024];
        ssize_t bytes_read;

        while ((bytes_read = read(sockfd, buffer, sizeof(buffer))) > 0) {
            fwrite(buffer, 1, bytes_read, file);
            printf("/");
            fflush(stdout);
        }
        fclose(file);

        printf("\nDone ^_^\n");
    } else {
        printf("There's no file with that name -_-\n");
    }

    close(sockfd);
    return 0;
}
