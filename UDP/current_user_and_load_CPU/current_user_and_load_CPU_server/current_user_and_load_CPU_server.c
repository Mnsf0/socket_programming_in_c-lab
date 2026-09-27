#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFSIZE 1024

typedef struct current_load {
   int current_user;
   double current_CPU_load;

}current_load;

double cpu_load(void) {
    FILE *fptr;
    fptr = fopen("/proc/loadavg", "r");

    if (fptr == NULL) {
        perror("Read-operation failed");
        return -1;
    }

    double load;
    fscanf(fptr, "%lf", &load);
    fclose(fptr);

  return load;
}

int logged_in_users(void) {
    FILE *fptr;
    fptr = popen("/usr/bin/who | /usr/bin/tr -s ' ' | /usr/bin/cut -d ' ' -f 1 | /usr/bin/sort -u | /usr/bin/wc -l", "r");

    if (fptr == NULL) {
        perror("Failed to execute user count");
        return -1;
    }

    int number_of_users;
    fscanf(fptr, "%d", &number_of_users);

    pclose(fptr);

  return number_of_users;
}

void updateLoadStats(current_load *stats) {
    stats->current_user = logged_in_users();
    stats->current_CPU_load = cpu_load();
}

static int check_port(const char *s) {
    // strtol() would also accept leading whitespace and +, - signs
    if (!isdigit((unsigned char)s[0]))
        return -1;

    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v < 1 || v > 65535)
        return -1;
    return (int)v;
}

static int check_IP_version(const char *ip, int port, struct sockaddr_storage *addr, socklen_t *len) {
    struct sockaddr_in  *addr4 = (struct sockaddr_in *)addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET, ip, &addr4->sin_addr) == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons((uint16_t)port);
        *len = sizeof(*addr4);
        return 0;
    }

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET6, ip, &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons((uint16_t)port);
        *len = sizeof(*addr6);
        return 0;
    }

    return -1;
}

int main(int argc, char** argv) {

    if(argc != 3){
        fprintf(stderr, "[-]Usage: %s <IP address > <PORT number>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    int local_port = check_port(argv[2]);
    if (local_port < 0) {
        fprintf(stderr, "[-]PORT (must be 1-65535)\n");
        exit(EXIT_FAILURE);
    }

     struct sockaddr_storage local_addr, client_addr;
     socklen_t local_addr_len, client_addr_len;

    if (check_IP_version(argv[1], local_port, &local_addr, &local_addr_len) < 0) {
       fprintf(stderr, "[-] Invalid local IP address\n");
       exit(EXIT_FAILURE);
    }


    int sockfd = socket(local_addr.ss_family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[-] SOKCET");
        exit(EXIT_FAILURE);
    }  


    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt(SO_REUSEADDR)");
        close(sockfd);
        exit(EXIT_FAILURE);
    }


  #ifdef SO_REUSEPORT
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
          perror("[-] setsockopt(SO_REUSEPORT)");
          close(sockfd);
          exit(EXIT_FAILURE);
      }
  #endif

      if (bind(sockfd, (struct sockaddr *)&local_addr, local_addr_len) < 0) {
          perror("[-] BIND");
          close(sockfd);
          exit(EXIT_FAILURE);
      }
     struct current_load *pStats = malloc(sizeof(struct current_load));

    while(1){

    char buff[BUFFSIZE];
    client_addr_len = sizeof(client_addr);
    int recv = recvfrom(sockfd, &buff, BUFFSIZE -1,0, (struct sockaddr *)&client_addr, &client_addr_len );
        
    if(recv < 0){
      fprintf(stderr, "[-]RECV");
      exit(EXIT_FAILURE);
    }

    fprintf(stdout, "recv bytes...: %d\n",recv);

    updateLoadStats(pStats);

    printf("\tSending (load): %lf\n", pStats->current_CPU_load);
    printf("\tSending (users): %d\n", pStats->current_user);

    int sent = sendto(sockfd, (current_load*) pStats, sizeof(*pStats),0, (struct sockaddr*)& client_addr, client_addr_len );

    if(sent < 0){
      fprintf(stderr, "[-]SEND]\n");
      exit(EXIT_FAILURE);
    }
   
    printf("sent bytes...: %d\n",sent);
    
  }

  close(sockfd);
  return 0;
}
