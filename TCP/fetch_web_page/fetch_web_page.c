#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <sys/socket.h>
#include <netdb.h>     
#include <unistd.h>
#include <fcntl.h>      
#include <sys/time.h> 

#define CHUNK_SIZE 1024

int recv_timeout(int s, int timeout)
{
    int size_recv, total_size = 0;
    struct timeval begin, now;
    char chunk[CHUNK_SIZE];
    double timediff;
    
   // fcntl() is a function used to overwrite file descriptor like socket *_* could imaged that!@#@#!#@#
  // s the socket, F_SETFL that tells we want to update the socket and allowe some configuration
   // O_NONBLOCK why we use this? there's some functions we called blocking functions what does that mean?
   // these function stops the code until getting somthing from other side, which is a problem beacuse there's
  // a chance that it won't get anything and if that happened congrats your code is broken 
  // to avoide this we put these funciton is non-blocking status
    fcntl(s, F_SETFL, O_NONBLOCK);
    gettimeofday(&begin, NULL);

    while (1)
    {
    //all of these is just an equation for how we want the chunks arrives and handle any unexepted cases
        gettimeofday(&now, NULL);
    // I won't lie I take the equation form AI -_-, I'm glad that I don't coding at 90's 
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);

    // here the program decide wait for another duration or that's enough let's get out
    // if we got some data and the time end anything is better from nothing get out soldier
        if (total_size > 0 && timediff > timeout)
        {
            break;
        }
         // if not wait for the double of the perioed extra job soldier
        else if (timediff > timeout * 2)
        {
         // clear the buffer 
            memset(chunk, 0, CHUNK_SIZE);
            if ((size_recv = recv(s, chunk, CHUNK_SIZE, 0)) < 0)
            {
        // don't be in rush wait a little -_- 
                usleep(100000);
            }
            else
            {
        // always add what we got to the total
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
    int socketfd;
    struct addrinfo hints, *res;
    char message[512];

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <DOMAIN_OR_IP>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *target = argv[1]; // store teh domain or the IP provided by the user

    
  // set up the certirea for connection with webstie 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       
    hints.ai_socktype = SOCK_STREAM; 

    
    // check the website, target (the domain naem we are looking to), 
    // 80 is the port for HTTP connections,
    // res: you can conjur it up like a list to your system to how it can connect to the web
    // having info like IP, protcol type but in a language that computer can understadn
    int status = getaddrinfo(target, "80", &hints, &res);
    if (status != 0)
    // why using fprintf instead of printf? 
    // two reasons, 1- printf send the outpuot to stdout, which is not accurate for this case 
    // 2-because we using gai_strerror(): but why?
    // gai_strerror convert status from numerical value to a human readable message
    {
        fprintf(stderr, "DNS resolution failed: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // setting the sokcet nothing special 
    socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1)
    {
    //handle erorrs like usuall -_-., I fell board that I need to repeat this three line 1456561 times on one code I must start using snippets 
        perror("Could not create socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Connect to the website most basic thing in life -_-.
    if (connect(socketfd, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("Connect error");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Address info is no longer needed after connection setup, but why?
    // because it's allocated in the heap so it's better to earsed it to avoid memory leak ^_^
    freeaddrinfo(res);
    puts("Connected\n");

    // why using snprintf simply because it has buffer the data is not going directly to the stdout
    // again why would we need this feature in first place? we don't want to display the HTTP request on the terminal screen
    // we need to build and store the request string inside the message array so the send() 
    // function can transmit it across the socket connection
    snprintf(message, sizeof(message),
             "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n"
             ,target);

    // normal send infor nothing special /-_-
    if (send(socketfd, message, strlen(message), 0) < 0)
    {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }

    puts("Data Sent\n");

    // Receive incoming data
    int total_recv = recv_timeout(socketfd, 4);

  // finally I can take a break ^_^
    printf("\n\nDone. Received a total of %d bytes\n\n", total_recv);
    close(socketfd);

    return 0;
}
