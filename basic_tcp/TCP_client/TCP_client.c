#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

int main() {

	int networkSocket ;
	networkSocket = socket(AF_INET, SOCK_STREAM ,0);
	
	struct sockaddr_in ClientSocket;
	ClientSocket.sin_family = AF_INET;
	ClientSocket.sin_port = htons(7897);
	ClientSocket.sin_addr.s_addr = INADDR_ANY;

	int connectionStatu = connect(networkSocket, (struct sockaddr *) &ClientSocket, sizeof(ClientSocket));

	if(connectionStatu == -1) {
		printf("something goes wrong\n");
	}

	char SocketResponse[512];
	recv(networkSocket, &SocketResponse, sizeof(SocketResponse), 0);

	printf("the message that come from the servre is: %s\n", SocketResponse);

	close(networkSocket);


	return 0;
}
