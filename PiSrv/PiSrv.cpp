/*
PiSrv, a small TCP server for the raspberry Pi - Peter Lockett 0901632
*/
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
// #include <curl/curl.h>

#define MAX_SIZE 50 // client_indexmum character size
#define NUMCONN 50 
#define MSG_CONNECTED	(0x41)	//A
#define MSG_SUCCESSFUL	(0x42)	//B

struct Cli_Info
{
	int socknum;
	FILE *f;
	// Might want to put in the disconnect time here to sore it
};

int recieve_file(int socket, char buff[])
{
	FILE *f = fopen("tmp.json", "w"); // append socket number to this filename just to make sure no other client sending a smaller file will overwrite.
	fwrite(buff,1, sizeof(buff), f );

	printf("Received:- %s \n", buff);
	// TODO : send message back to client that their message has been rec'd
	//send(Sockets.at(i), MSG_CONNECTED, strlen(MSG_CONNECTED), 0)
}
int main()
{
	int connections = 0;
	fd_set tempset, Masterset;  // descriptor set to be monitored
	struct sockaddr_in serv_addr, client_addr;
	char buff[MAX_SIZE];

	// CURL Stuff
	// CURL *curl;
	// curl = curl_easy_init();

	timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 500000;
	std::vector<int> Sockets;
	std::vector<Cli_Info> Clients;
	/* TODO: change this : */ for (int i = 0; i < NUMCONN; i++)Sockets.push_back(0);

	Sockets.at(0) = socket(AF_INET, SOCK_STREAM, 0);

	if(Sockets.at(0) < 0)
	{
		printf("Failed creating socket\n");
		return 0;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));


	serv_addr.sin_family = AF_INET; // IPv4 Address Family
	serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
	serv_addr.sin_port = htons(5099);


	if (bind(Sockets.at(0), (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Failed to bind on port\n");
		return 0;
	}

	if(listen(Sockets.at(0), 5) == -1) // -1 is on error, 0 is successful
	{
		printf("Error: Unable to listen!\n");
		return 0;
	}

	// Clear file descriptor sets
	FD_ZERO(&tempset);
	FD_ZERO(&Masterset); // initialize the descriptor set to be monitored to empty
	FD_SET(Sockets.at(0), &Masterset); // Add the listening socket in the MASTER set


	printf("Server setup - OK\n");
	printf("Server is now listening for connections!\n");
	while(true) // main server loop
	{
		// assign all currently monitored descriptor set to a local variable. This is needed because select
		// will overwrite this set and we will lose track of what we originally wanted to monitor.
		tempset = Masterset;

		// Look for socket activity
		select(FD_SETSIZE, &tempset, NULL, NULL, &timeout);


		if(FD_ISSET(Sockets.at(0), &tempset)) // new client connection
		{
			int size = sizeof(client_addr);
			int Accepted, SockNUM;
			Accepted = 0;

			// Add that new client's socket on to a std::vector
			// Sockets.push_back(accept(listen_desc, (struct sockaddr *)&client_addr, (socklen_t*)&size));
			for (int i = 1; i < NUMCONN; i++)
			{
				if (Sockets.at(i) == 0)
				{
					SockNUM = i;
					Sockets.at(SockNUM) = accept(Sockets.at(0), (struct sockaddr *)&client_addr, (socklen_t*)&size);

					printf("Accepted new client on socket %d\n", i);
					Accepted = 1;
					break;
				}
			}

			if (Accepted == 1)
			{
				printf("Confirming client connection\n");
				// TODO: Send message back so the client know it has connected

				// Put the transmission socket in the master set
				FD_SET(Sockets.at(SockNUM), &Masterset);
				// send (Sockets.at(SockNUM), buff, sizeof(buff), MSG_CONNECTED);
				// snprintf(buff, sizeof(buff), "%c", MSG_CONNECTED);
				// send(Sockets.at(SockNUM), buff, sizeof(buff), 0);
				connections++;
				printf("Number of connected clients = %d \n\n", connections);
			}
		}

		// Process all connection sockets, start at 1 because the listening socket is at 0
		for (int i = 1; i < NUMCONN; i++)
		{
			if (Sockets.at(i) > 0)
			{
				if (FD_ISSET(Sockets.at(i), &tempset))
				{
					int num_bytes = recv(Sockets.at(i), buff, sizeof(buff), 0);

					// If the client i has connected and is sending byte data to the server, receive the data on that socket and write it to a file
					if (num_bytes > 0)
					{
						buff[num_bytes] = '\0';
						recieve_file(Sockets.at(i), buff);
					}

					if(num_bytes == 0)  // connection was closed by client
					{
						close(Sockets.at(i));
						FD_CLR(Sockets.at(i), &Masterset);
						Sockets.at(i) = 0;
						printf("Client closed connection\n");
						connections--;
					}
				}
			}
		}	
	}


	for (int i = 0; i < Sockets.size(); i++) close(Sockets.at(i)); // Close all sockets
	return 0;
}