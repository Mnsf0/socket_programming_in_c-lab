//Explain what did I modify in this version
//1-use different number with exit(#), but why? to where exactly the
//program falied and troublshoting, and for the same reason 
//I use this exit(#) over exit(EXIT_FAILURE), but in case the code gonna go to the production 
//and it will be use by windows systems I would use exit(EXIT_FAILURE)
//2- reuse the port after we done from the program
//3-make sure to handle erorr at any level of the code, 
//and in case an erorr happened I must close the socket descriptor
//4- close both sokcets client and Sokcet at the end of the code
//5- there was a bug where I wrote ntohs() instead of htons()


#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/in.h>


int main() {

	char data[512] = "helloooooooooooooooooooooooooo\n" ;
	int Socket;

        Socket = socket(AF_INET, SOCK_STREAM, 0);
  //check for any failure
  if(Socket < 0){
    perror("[-]SOCKET\n");
    exit(1);
  }
fprintf(stdout,"[+]SOCKET\n");

//allow to use the port again
int opt = 1 ;
if(setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
    perror("[-]SetSocketOpt\n");
    close(Socket);
    exit(2);
}

	struct sockaddr_in Dsocket;
	Dsocket.sin_family = AF_INET;
	Dsocket.sin_port = htons(7897); // it was ntohs()
	Dsocket.sin_addr.s_addr = INADDR_ANY;

    if(bind(Socket, (struct sockaddr *) &Dsocket, sizeof(Dsocket) ) < 0){
    perror("[-]BIND\n");
    close(Socket);
    exit(3);
  }
fprintf(stdout,"[+]BIND\n");


	if(listen(Socket, 2) < 0){
    perror("[-]LISTEN\n");
    close(Socket);
    exit(4);
  }
fprintf(stdout,"[+]LISTEN\n");

	int client = accept(Socket, NULL, NULL);
  if(client < 0){
    perror("[-]ACCEPT\n");
    close(Socket);
    exit(5);
  }
fprintf(stdout,"[+]ACCEPT\n");


	if(send(client, data, sizeof(data), 0) < 0 ){
    perror("[-]SEND\n");
    close(Socket);
    exit(6);
  }
fprintf(stdout,"[+]SEND\n");

	close(Socket);
  close(client);

	return 0;

}
