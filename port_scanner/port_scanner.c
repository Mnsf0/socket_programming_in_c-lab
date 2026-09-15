#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>


int main(int argc, char **argv) {

    struct hostent *host;
    int sockfd;
    char hostname[100];
    struct sockaddr_in sockaddress;

  
    printf("Enter hostname or IP: ");

   // why I used fgets instead of scanf()? to avoide any buffer overflow, the scanf() 
   // reads inputs without checking for the bounds and write directly to the variable 
    fgets(hostname, sizeof(hostname), stdin);
    hostname[strcspn(hostname, "\n")] = '\0'; // Remove newline character

    // get the bounds
    int start, end;

    printf("Note: if you to check only one port, enter the port number twice at start and end\n");
    printf("\nEnter start port number: ");
    scanf("%d", &start);
    printf("Enter end port number: ");
    scanf("%d", &end);

    // Initialize the sockaddr_in structure
    memset(&sockaddress, 0, sizeof(sockaddress));
    sockaddress.sin_family = AF_INET;

    // check if the hostname is a direct IP address
    if (isdigit(hostname[0])) {
        printf("Doing inet_addr...\n");
    // what does inet_addr() do? simply the input of the user is readable text
    // 204.154.75.36 something like that, while this function convert it to 32 bit
    // binary integer in network byte order 
        sockaddress.sin_addr.s_addr = inet_addr(hostname);
       } else {
        // resolve the hostname to an IP addreiss
        printf("Doing gethostbyname...\n");
    // you can imagen this as DNS lookup mechanism it check the IP for the provided domain name
        host = gethostbyname(hostname);
        if (host == NULL) {
            perror("hostname");
            exit(2);
        }
    //simply this line take the output of gethostbyname() and put it into sockfd
        strncpy((char*)&sockaddress.sin_addr,
               (char*)host->h_addr, sizeof(sockaddress.sin_addr));
    }

    int i;
      printf("Starting the portscan loop:\n");
      // get through each port in the range
    for (i = start; i <= end; i++) {

        sockaddress.sin_port = htons(i);

        // create a socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("[-]Socket\n");
            exit(1);
        }

     int err ;
        // normal connection -_-
        err = connect(sockfd, (struct sockaddr*)&sockaddress, sizeof(sockaddress));

    
    //-ECONNREFUSED: The target machine is online and received your request, but no application is listening on that port. The host's operating system actively rejected your connection by sending back a TCP RST (Reset) packet. This indicates a closed port.
    //-ETIMEDOUT:Your system sent a packet, but received no reply before the socket timer expired. This usually happens when a firewall drops incoming or outgoing packets without sending a reply. This indicates a filtered port.
    //-EHOSTUNREACH: An intermediate router or local gateway could not find a path to the target IP address, or the host is entirely offline/unplugged. This indicates an unreachable host or filtered network.
    
    if (err < 0) {
    if (errno == ECONNREFUSED) {
        printf("%-5d closed\n", i);
      } else if (errno == ETIMEDOUT || errno == EHOSTUNREACH) {
        printf("%-5d filtered\n", i);
      } else {
        printf("%-5d error: %s\n", i, strerror(errno));
      }
    } else {
    printf("%-5d open\n", i);
    }
        // usually we close the socket at the end of the program, but in this case it's differet
    // why? beacuse why making differet socket to check different ports so each iteration mean a new socket and before 
    // we done with it we need to close it 
    // I wise man once say never forget to close your sokcet <-_-> 
        close(sockfd);
    }
 

    fflush(stdout);
    return 0;
}
