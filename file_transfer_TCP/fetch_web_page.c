#include <stdio.h>      // printf, fprintf, perror
#include <stdlib.h>     // exit, EXIT_FAILURE
#include <string.h>     // strlen, memset
#include <sys/socket.h> // socket, connect
#include <netdb.h>      // getaddrinfo, freeaddrinfo, struct addrinfo
#include <unistd.h>     // usleep, close
#include <fcntl.h>      // fcntl
#include <sys/time.h>   // gettimeofday

#define CHUNK_SIZE 1024

int recv_timeout(int s, int timeout)
{
    int size_recv, total_size = 0;
    struct timeval begin, now;
    char chunk[CHUNK_SIZE];
    double timediff;

    fcntl(s, F_SETFL, O_NONBLOCK);
    gettimeofday(&begin, NULL);

    while (1)
    {
        gettimeofday(&now, NULL);
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);

        if (total_size > 0 && timediff > timeout)
        {
            break;
        }
        else if (timediff > timeout * 2)
        {
            memset(chunk, 0, CHUNK_SIZE);
            if ((size_recv = recv(s, chunk, CHUNK_SIZE, 0)) < 0)
            {
                usleep(100000);
            }
            else
            {
                total_size += size_recv;
                printf("%s", chunk);
                gettimeofday(&begin, NULL);
            }
        }
    }
  return total_size;
}

int main(int argc, char *argv[])
{
    int socket_desc;
    struct addrinfo hints, *res;
    char message[512];

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <DOMAIN_OR_IP>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *target = argv[1];

    // Configure criteria for target resolution
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream socket

    // getaddrinfo converts IP string OR resolves domain name automatically
    int status = getaddrinfo(target, "80", &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "DNS resolution failed: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // Create socket using the resolved protocol details
    socket_desc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socket_desc == -1)
    {
        perror("Could not create socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Connect to target address
    if (connect(socket_desc, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Connect error");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Address info is no longer needed after connection setup
    freeaddrinfo(res);
    puts("Connected\n");

    // Format HTTP request using the passed target parameter as the Host header
    snprintf(message, sizeof(message),
             "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
             target);

    if (send(socket_desc, message, strlen(message), 0) < 0)
    {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }
    puts("Data Sent\n");

    // Receive incoming data
    int total_recv = recv_timeout(socket_desc, 4);

    printf("\n\nDone. Received a total of %d bytes\n\n", total_recv);
    close(socket_desc);
    return 0;
}
