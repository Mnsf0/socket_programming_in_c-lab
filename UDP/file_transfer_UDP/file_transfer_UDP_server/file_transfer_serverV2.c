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
#define MAX_FILE_BYTES (100 * 1024 * 1024)

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

//the purpose of this function to ckeck if the set token is valid or not 
//based on the requirements 
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

//this funciton use to compares between two token to check if the client token is valid or not
static int ct_equal(const char *a, const char *b, size_t n) {
    unsigned char diff = 0;
    for (size_t i = 0; i < n; i++) {
        diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return diff == 0;
}

static const char *server_token;
static size_t token_len;
 
/* Returns 1 only if buf is exactly "AUTH <token>"
 * Validates the message format and length first, then calls ct_equal()
 * to compare the secret token in constant time, so response timing
 * can't reveal how much of a guessed token was correct. */
static int check_auth(const char *buf, size_t len) {
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        len--;
    if (len != 5 + token_len)
        return 0;
    if (memcmp(buf, "AUTH ", 5) != 0)
        return 0;
    return ct_equal(buf + 5, server_token, token_len);
}

/* Returns 1 if a and b have the same address family, IP address, and port,
  0 otherwise, used to make sure later UDP datagrams come from the
  authenticated client and not from another sender */
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

static int setup_ipv4_socket(const char *ip, int port, const char *message) {
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) {
        perror("socket fail");
        return -1;
    }
 
    bzero(&sock_serv4, sizeof(sock_serv4));
    sock_serv4.sin_family = AF_INET;
    sock_serv4.sin_port = htons(port);
    if (ip == NULL)
        sock_serv4.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, ip, &sock_serv4.sin_addr) != 1) {
        fprintf(stderr, "setup_ipv4_socket: invalid IPv4 address '%s'\n", ip);
        close(sfd);
        return -1;
    }
 
    if (bind(sfd, (struct sockaddr *)&sock_serv4, sizeof(sock_serv4)) == -1) {
        perror("bind fail");
        close(sfd);
        return -1;
    }
 
    printf("%s: listening on %s:%d (IPv4)\n", message, ip ? ip : "0.0.0.0", port);
    return sfd;
}
 
static int setup_ipv6_socket(const char *ip, int port, const char *message) {
    int off = 0;
    int sfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sfd == -1) {
        perror("socket fail");
        return -1;
    }
 
    bzero(&sock_serv6, sizeof(sock_serv6));
    sock_serv6.sin6_family = AF_INET6;
    sock_serv6.sin6_port = htons(port);
    if (ip == NULL)
        sock_serv6.sin6_addr = in6addr_any;
    else if (inet_pton(AF_INET6, ip, &sock_serv6.sin6_addr) != 1) {
        fprintf(stderr, "setup_ipv6_socket: invalid IPv6 address '%s'\n", ip);
        close(sfd);
        return -1;
    }
 
    /* Allow IPv4 clients too when bound to the wildcard "::" (not always supported) */
    if (setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == -1)
        perror("setsockopt(IPV6_V6ONLY) warning");
 
    if (bind(sfd, (struct sockaddr *)&sock_serv6, sizeof(sock_serv6)) == -1) {
        perror("bind fail");
        close(sfd);
        return -1;
    }
 
    printf("%s: listening on [%s]:%d (IPv6)\n", message, ip ? ip : "::", port);
    return sfd;
}
 
int create_socket_from_ip(const char *ip, int port, const char *message) {
    struct sockaddr_in  test4;
    struct sockaddr_in6 test6;
 
    if (ip == NULL) {
        return setup_ipv4_socket(ip, port, message);   /* default to IPv4 wildcard */
    } else if (inet_pton(AF_INET, ip, &test4.sin_addr) == 1) {
        return setup_ipv4_socket(ip, port, message);
    } else if (inet_pton(AF_INET6, ip, &test6.sin6_addr) == 1) {
        return setup_ipv6_socket(ip, port, message);
    } else {
        fprintf(stderr, "create_socket_from_ip: '%s' is not a valid IPv4 or IPv6 address\n", ip);
        return -1;
    }
}
static int check_port(const char* port){
  char* endptr;
  errno = 0;
  //first bug: strtol() skips white space and +,- signs at the first, 
  //so if the user try to enter 154 or -154 both are valid, and any lead zero will pass the test
  //second bug: is not really safe to terminate proccess in outer function
  if(!isdigit((unsigned char)port[0]) || port[0] == '0'){
    return -1;
  }
  long val = strtol(port, &endptr,10);

  if(errno != 0 || *endptr != '\0' || endptr == port){
      fprintf(stderr, "[-]PORT\n");
      return -1;
    }  
  if(val < 1 || val > 65535){
      fprintf(stderr, "[-]PORT\n");
      return -1;
    }
  return (int)val;
 }

int main(int argc, char **argv) {
    int fd, sfd;

    char buf[BUFFERT];
    off_t count = 0, n;
    char filename[256];
    unsigned int l = sizeof(struct sockaddr_storage);

    char port_str[16];
    char ip_str[64];
    const char *ip = NULL;
    int port;
    struct sockaddr_storage auth_clt;
    struct timeval t_start, t_last, t_now, t_delta;

    time_t intps;
    struct tm *tmi;

    if (argc != 1) {
        printf("Error usage : %s \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Set the port (1-65535): ");

    //It forces any text waiting in the output buffer to be written to the screen immediately
    //why is that? because; printf doesn’t write straight to the terminal, it puts the text in a buffer,
    //and the buffer is only written out when it fills up, when the program exits
    fflush(stdout);

    if (fgets(port_str, sizeof(port_str), stdin) == NULL) {
        fprintf(stderr, "Port not set\n");
        exit(EXIT_FAILURE);
    }

    if (strchr(port_str, '\n') == NULL && strlen(port_str) == sizeof(port_str) - 1) {
        fprintf(stderr, "[-]PORT\n");   /* input too long to be a valid port */
        exit(EXIT_FAILURE);
    }

    port_str[strcspn(port_str, "\r\n")] = '\0';
 
    port = check_port(port_str);
    if (port == -1)
        exit(EXIT_FAILURE);
 
    printf("Set the bind IP address (IPv4 or IPv6, empty = IPv4 wildcard): ");
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
    ip = (ip_str[0] == '\0') ? NULL : ip_str;

    server_token = getenv("CHAT_TOKEN");
    if (server_token == NULL || !is_valid_token(server_token)) {
        fprintf(stderr, "CHAT_TOKEN must be set (%d-%d characters: letters, digits, '_', '-', '.')\n",
                TOKEN_MIN, TOKEN_MAX);
        exit(EXIT_FAILURE);
    }

    token_len = strlen(server_token);
 
    sfd = create_socket_from_ip(ip, port, "Server");
    if (sfd == -1)
        exit(EXIT_FAILURE);
   
   
   //this while loop to check the authentication
    while (1) {
    //reset the buffer
        bzero(&buf, BUFFERT);
        l = sizeof(struct sockaddr_storage);
    //receiving the token
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
    // removing new lines
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
    //check token validation
        if ((size_t)n != strlen(buf) || !is_valid_token(buf)) {
            fprintf(stderr, "Malformed request ignored\n");
            continue;
        }
    //compare the token 
        if (strcmp(buf, server_token) != 0) {
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

    while (1) {
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

    if (n == 0)
      break;

    t_last = t_now;
    printf("%jd of data received \n", (intmax_t)n);

     if (count + n > MAX_FILE_BYTES) {
       fprintf(stderr, "File size limit exceeded\n");
       exit(EXIT_FAILURE);
     }
     count += n;
     if (write(fd, buf, n) == -1) {
       perror("write fail");
       exit(EXIT_FAILURE);
     }
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
