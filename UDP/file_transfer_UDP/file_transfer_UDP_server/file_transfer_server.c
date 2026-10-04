#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <inttypes.h>

#define BUFFERT 512
#define TOKEN_MIN 8
#define TOKEN_MAX 64
#define IDLE_LIMIT 10

struct sockaddr_in6 sock_serv6;
struct sockaddr_in  sock_serv4;
struct sockaddr_storage clt;

int duration(struct timeval *start, struct timeval *stop, struct timeval *delta) {
    suseconds_t microstart, microstop, microdelta;

    microstart = (suseconds_t)(1000000 * (start->tv_sec)) + start->tv_usec;
    microstop = (suseconds_t)(1000000 * (stop->tv_sec)) + stop->tv_usec;
    microdelta = microstop - microstart;
    delta->tv_usec = microdelta % 1000000;
    delta->tv_sec = (time_t)(microdelta / 1000000);

    if ((*delta).tv_sec < 0 || (*delta).tv_usec < 0)
        return -1;
    else
        return 0;
}

int is_valid_token(const char *s) {
    size_t len = strlen(s);

    if (len < TOKEN_MIN || len > TOKEN_MAX)
        return 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.')
            return 0;
    }
    return 1;
}

int same_client(struct sockaddr_storage *a, struct sockaddr_storage *b) {
    if (a->ss_family != b->ss_family)
        return 0;

    if (a->ss_family == AF_INET) {
        struct sockaddr_in *x = (struct sockaddr_in *)a;
        struct sockaddr_in *y = (struct sockaddr_in *)b;
        return x->sin_port == y->sin_port &&
               x->sin_addr.s_addr == y->sin_addr.s_addr;
    }

    if (a->ss_family == AF_INET6) {
        struct sockaddr_in6 *x = (struct sockaddr_in6 *)a;
        struct sockaddr_in6 *y = (struct sockaddr_in6 *)b;
        return x->sin6_port == y->sin6_port &&
               memcmp(&x->sin6_addr, &y->sin6_addr, sizeof(x->sin6_addr)) == 0;
    }

    return 0;
}

int create_server_socket(int port) {
    int l;
    int sfd;
    int off = 0;

    sfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sfd != -1) {
        l = sizeof(struct sockaddr_in6);
        bzero(&sock_serv6, l);

        sock_serv6.sin6_family = AF_INET6;
        sock_serv6.sin6_port = htons(port);
        sock_serv6.sin6_addr = in6addr_any;

        if (setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == 0 &&
            bind(sfd, (struct sockaddr *)&sock_serv6, l) == 0) {
            printf("Listening on port %d (IPv6 + IPv4)\n", port);
            return sfd;
        }
        close(sfd);
    }

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) {
        perror("socket fail");
        return -1;
    }

    l = sizeof(struct sockaddr_in);
    bzero(&sock_serv4, l);

    sock_serv4.sin_family = AF_INET;
    sock_serv4.sin_port = htons(port);
    sock_serv4.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sfd, (struct sockaddr *)&sock_serv4, l) == -1) {
        perror("bind fail");
        close(sfd);
        return -1;
    }

    printf("Listening on port %d (IPv4 only, IPv6 not available)\n", port);
    return sfd;
}

int main(int argc, char **argv) {
    int fd, sfd;

    char buf[BUFFERT];
    off_t count = 0, n;
    char filename[256];
    unsigned int l = sizeof(struct sockaddr_storage);

    char token[TOKEN_MAX + 2];
    struct sockaddr_storage auth_clt;
    struct timeval t_start, t_last, t_now, t_delta;

    time_t intps;
    struct tm *tmi;

    if (argc != 2) {
        printf("Error usage : %s <port_serv>\n", argv[0]);
        exit(EXIT_FAILURE);
       }

    printf("Set the token (%d-%d chars: letters, digits, _ - .): ", TOKEN_MIN, TOKEN_MAX);
    fflush(stdout);
    if (fgets(token, sizeof(token), stdin) == NULL) {
        fprintf(stderr, "Token not set\n");
        exit(EXIT_FAILURE);
    }
    token[strcspn(token, "\r\n")] = '\0';

    if (!is_valid_token(token)) {
        fprintf(stderr, "Invalid token: use %d-%d characters, only letters, digits, '_', '-', '.'\n",
                TOKEN_MIN, TOKEN_MAX);
        exit(EXIT_FAILURE);
    }

    sfd = create_server_socket(atoi(argv[1]));
    if (sfd == -1)
        exit(EXIT_FAILURE);

    while (1) {
        bzero(&buf, BUFFERT);
        l = sizeof(struct sockaddr_storage);
        n = recvfrom(sfd, &buf, BUFFERT - 1, 0, (struct sockaddr *)&clt, &l);

        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("read fails");
            exit(EXIT_FAILURE);
        }
        if (n == 0 || (clt.ss_family != AF_INET6 && clt.ss_family != AF_INET)) {
            fprintf(stderr, "Empty or unknown request ignored\n");
            continue;
        }
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
        if ((size_t)n != strlen(buf) || !is_valid_token(buf)) {
            fprintf(stderr, "Malformed request ignored\n");
            continue;
        }
        if (strcmp(buf, token) != 0) {
            fprintf(stderr, "Wrong token, request ignored\n");
            continue;
        }
        break;
    }
    auth_clt = clt;
    printf("Token accepted\n");

    intps = time(NULL);
    tmi = localtime(&intps);
    bzero(filename, 256);
    sprintf(filename, "clt.%d.%d.%d.%d.%d.%d", tmi->tm_mday, tmi->tm_mon + 1, 1900
            + tmi->tm_year, tmi->tm_hour, tmi->tm_min, tmi->tm_sec);
    printf("Creating the output file : %s\n", filename);
    if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0600)) == -1) {
        perror("open fail");
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = IDLE_LIMIT;
    tv.tv_usec = 0;
    if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("setsockopt(SO_RCVTIMEO) fail");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&t_start, NULL);
    t_last = t_start;

    l = sizeof(struct sockaddr_storage);
    bzero(&buf, BUFFERT);
    n = recvfrom(sfd, &buf, BUFFERT, 0, (struct sockaddr *)&clt, &l);

    while (n) {
    gettimeofday(&t_now, NULL);

    if (n == -1) {
      if (errno == EINTR) {
        l = sizeof(struct sockaddr_storage);
        n = recvfrom(sfd, &buf, BUFFERT, 0, (struct sockaddr *)&clt, &l);
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        fprintf(stderr, "Connection timed out (no data for %d s)\n", IDLE_LIMIT);
      else
        perror("read fails");
      exit(EXIT_FAILURE);
     }

    if (duration(&t_last, &t_now, &t_delta) == -1 || t_delta.tv_sec >= IDLE_LIMIT) {
      fprintf(stderr, "Connection not valid (idle too long or clock error)\n");
      exit(EXIT_FAILURE);
     }

    if (!same_client(&clt, &auth_clt)) {
      fprintf(stderr, "Datagram from another sender ignored\n");
      l = sizeof(struct sockaddr_storage);
      n = recvfrom(sfd, &buf, BUFFERT, 0, (struct sockaddr *)&clt, &l);
      continue;
     }

    t_last = t_now;
    printf("%jd of data received \n", (intmax_t)n);

     count += n;
     write(fd, buf, n);
     bzero(buf, BUFFERT);
     l = sizeof(struct sockaddr_storage);
     n = recvfrom(sfd, &buf, BUFFERT, 0, (struct sockaddr *)&clt, &l);
    }

    gettimeofday(&t_now, NULL);
    if (duration(&t_start, &t_now, &t_delta) == 0)
        printf("Transfer duration : %ld.%06ld s\n", (long)t_delta.tv_sec, (long)t_delta.tv_usec);

    printf("Total number of bytes transferred : %jd \n", (intmax_t)count);

    close(sfd);
    close(fd);
    return 0;

}
