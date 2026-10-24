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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <inttypes.h>

#define BUFFERT 512             /* must not exceed the server's BUFFERT */
#define TOKEN_MIN 8
#define TOKEN_MAX 64
#define TOKEN_DELAY_US 200000   /* give the server time to open the output file */
#define PACKET_DELAY_US 1000    /* pacing: the server has no ACKs, so don't flood it */
#define EOF_REPEAT 3            /* the empty "end of file" datagram is sent a few times */

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

/* Same rules as the server: 8-64 characters, letters, digits, '_', '-', '.' */
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

/* Same port rules as the server: digits only, no leading zero, 1-65535 */
static int check_port(const char *port) {
    char *endptr;
    errno = 0;

    if (!isdigit((unsigned char)port[0]) || port[0] == '0') {
        fprintf(stderr, "[-]PORT\n");
        return -1;
    }
    long val = strtol(port, &endptr, 10);

    if (errno != 0 || *endptr != '\0' || endptr == port) {
        fprintf(stderr, "[-]PORT\n");
        return -1;
    }
    if (val < 1 || val > 65535) {
        fprintf(stderr, "[-]PORT\n");
        return -1;
    }
    return (int)val;
}

/* Builds the destination address (IPv4 or IPv6) the same way the server detects it */
static int build_server_addr(const char *ip, int port,
                             struct sockaddr_storage *dst, socklen_t *dlen) {
    struct sockaddr_in  *a4 = (struct sockaddr_in *)dst;
    struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)dst;

    bzero(dst, sizeof(*dst));

    if (inet_pton(AF_INET, ip, &a4->sin_addr) == 1) {
        a4->sin_family = AF_INET;
        a4->sin_port = htons(port);
        *dlen = sizeof(struct sockaddr_in);
        return 0;
    }
    if (inet_pton(AF_INET6, ip, &a6->sin6_addr) == 1) {
        a6->sin6_family = AF_INET6;
        a6->sin6_port = htons(port);
        *dlen = sizeof(struct sockaddr_in6);
        return 0;
    }
    fprintf(stderr, "build_server_addr: '%s' is not a valid IPv4 or IPv6 address\n", ip);
    return -1;
}

/* Sends one datagram (len may be 0), retrying if a signal interrupts the call */
static int send_datagram(int sfd, const char *data, size_t len,
                         const struct sockaddr *dst, socklen_t dlen) {
    ssize_t r;
    do {
        r = sendto(sfd, data, len, 0, dst, dlen);
    } while (r == -1 && errno == EINTR);

    if (r == -1) {
        perror("sendto fail");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int fd, sfd;

    char buf[BUFFERT];
    off_t count = 0;
    ssize_t n;

    char port_str[16];
    char ip_str[64];
    char path[256];
    int port;
    const char *token;
    size_t token_len;

    struct sockaddr_storage srv;
    socklen_t srv_len;
    struct stat st;
    struct timeval t_start, t_now, t_delta;

    if (argc != 1) {
        printf("Error usage : %s \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* ---- server IP ---- */
    printf("Set the server IP address (IPv4 or IPv6): ");
    fflush(stdout);

    if (fgets(ip_str, sizeof(ip_str), stdin) == NULL) {
        fprintf(stderr, "IP address not set\n");
        exit(EXIT_FAILURE);
    }
    if (strchr(ip_str, '\n') == NULL && strlen(ip_str) == sizeof(ip_str) - 1) {
        fprintf(stderr, "Invalid IP address (too long)\n");
        exit(EXIT_FAILURE);
    }
    ip_str[strcspn(ip_str, "\r\n")] = '\0';

    /* ---- server port ---- */
    printf("Set the server port (1-65535): ");
    fflush(stdout);

    if (fgets(port_str, sizeof(port_str), stdin) == NULL) {
        fprintf(stderr, "Port not set\n");
        exit(EXIT_FAILURE);
    }
    if (strchr(port_str, '\n') == NULL && strlen(port_str) == sizeof(port_str) - 1) {
        fprintf(stderr, "[-]PORT\n");
        exit(EXIT_FAILURE);
    }
    port_str[strcspn(port_str, "\r\n")] = '\0';

    port = check_port(port_str);
    if (port == -1)
        exit(EXIT_FAILURE);

    if (build_server_addr(ip_str, port, &srv, &srv_len) == -1)
        exit(EXIT_FAILURE);

    /* ---- file to send ---- */
    printf("Set the path of the file to send: ");
    fflush(stdout);

    if (fgets(path, sizeof(path), stdin) == NULL) {
        fprintf(stderr, "File path not set\n");
        exit(EXIT_FAILURE);
    }
    if (strchr(path, '\n') == NULL && strlen(path) == sizeof(path) - 1) {
        fprintf(stderr, "File path too long\n");
        exit(EXIT_FAILURE);
    }
    path[strcspn(path, "\r\n")] = '\0';

    if (path[0] == '\0') {
        fprintf(stderr, "File path is empty\n");
        exit(EXIT_FAILURE);
    }

    /* ---- token (must be the same CHAT_TOKEN the server uses) ---- */
    token = getenv("CHAT_TOKEN");
    if (token == NULL || !is_valid_token(token)) {
        fprintf(stderr, "CHAT_TOKEN must be set (%d-%d characters: letters, digits, '_', '-', '.')\n",
                TOKEN_MIN, TOKEN_MAX);
        exit(EXIT_FAILURE);
    }
    token_len = strlen(token);

    /* ---- open the file ---- */
    if ((fd = open(path, O_RDONLY)) == -1) {
        perror("open fail");
        exit(EXIT_FAILURE);
    }
    if (fstat(fd, &st) == -1 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "'%s' is not a regular file\n", path);
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* ---- one socket for the whole transfer: the server compares our IP and port ---- */
    sfd = socket(srv.ss_family, SOCK_DGRAM, 0);
    if (sfd == -1) {
        perror("socket fail");
        close(fd);
        exit(EXIT_FAILURE);
    }

    gettimeofday(&t_start, NULL);

    /* Phase 1: authentication, the first datagram is the raw token (no newline) */
    printf("Sending token to the server\n");
    if (send_datagram(sfd, token, token_len, (struct sockaddr *)&srv, srv_len) == -1) {
        close(sfd);
        close(fd);
        exit(EXIT_FAILURE);
    }
    usleep(TOKEN_DELAY_US);

    /* Phase 2: file content, up to BUFFERT bytes per datagram (never an empty one) */
    while ((n = read(fd, buf, BUFFERT)) != 0) {
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("read fail");
            close(sfd);
            close(fd);
            exit(EXIT_FAILURE);
        }

        if (send_datagram(sfd, buf, (size_t)n, (struct sockaddr *)&srv, srv_len) == -1) {
            close(sfd);
            close(fd);
            exit(EXIT_FAILURE);
        }

        count += n;
        printf("%jd of data sent \n", (intmax_t)n);
        usleep(PACKET_DELAY_US);
    }

    /* Phase 3: an empty datagram tells the server the file is finished */
    for (int i = 0; i < EOF_REPEAT; i++) {
        if (send_datagram(sfd, buf, 0, (struct sockaddr *)&srv, srv_len) == -1)
            break;
        usleep(PACKET_DELAY_US);
    }

    gettimeofday(&t_now, NULL);
    if (duration(&t_start, &t_now, &t_delta) == 0)
        printf("Transfer duration : %ld.%06ld s\n", (long)t_delta.tv_sec, (long)t_delta.tv_usec);

    printf("Total number of bytes sent : %jd \n", (intmax_t)count);

    close(sfd);
    close(fd);
    return 0;
}
