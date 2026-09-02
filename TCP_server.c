#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/in.h>


int main() {

	char data[512] = "helloooooooooooooooooo body what are you doing?????????\n" ;
	int Socket;
        Socket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in Dsocket;
	Dsocket.sin_family = AF_INET;
	Dsocket.sin_port = ntohs(7897);
	Dsocket.sin_addr.s_addr = INADDR_ANY;

        bind(Socket, (struct sockaddr *) &Dsocket, sizeof(Dsocket) );

	listen(Socket, 2);

	int client;
	client = accept(Socket, NULL, NULL);

	send(client, data, sizeof(data), 0);

	close(Socket);

	return 0;




}
