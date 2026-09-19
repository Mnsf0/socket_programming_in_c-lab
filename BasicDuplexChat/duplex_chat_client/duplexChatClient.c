#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>


int main(int argc, char** argv){

  //define socket
  int sockdf = socket(AF_INET,SOCK_STREAM,0);
  if(sockdf == -1){
    perror("[-]Socket\n");
    exit(EXIT_FAILURE);
  }

 //define address of socket
  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(15151);
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  //set the buffer for sending and receving
  char send_buffer[256], recv_buffer[256];
 
  //get process id to distinguish between child & parent process
  pid_t cpid;

  //using if statement to handle and declare the error for user
  if(connect(sockdf,(struct sockaddr*)& addr, sizeof(addr)) == -1){
    perror("[-]Connect\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]Connect\n");

  //implement fork to duplicate the process into child and parent
  //one for recv and the otehr for send
  cpid = fork();

  if(cpid < 0){
    perror("[-]Fork\n");
    close(sockdf);
    exit(EXIT_FAILURE);
  }

  if(cpid == 0){ // child process
   while(1){
      //use bzero to to initilization all the buffer to '\0'
    bzero(&recv_buffer, sizeof(recv_buffer));
     if(recv(sockdf,recv_buffer, sizeof(recv_buffer),0) <= 0){
        perror("[-]Recv\n"); // declare where the erorr happened
        exit(EXIT_FAILURE); // handle error
      }
     printf("\nServer: %s\n",recv_buffer); 

    }
  }else {// parent process
     while(1){
      bzero(&send_buffer,sizeof(send_buffer));
      printf("Type Message:");
      fgets(send_buffer,256,stdin);

      if(send(sockdf,send_buffer,strlen(send_buffer)+1,0) == -1){
        perror("[-]Send\n");
        exit(EXIT_FAILURE);
      }
      printf("Message sent! \n");
    }

  }

  close(sockdf);

  return 0;

}
