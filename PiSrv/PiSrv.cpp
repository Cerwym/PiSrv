/*
PiSrv, a small TCP server for the raspberry Pi - Peter Lockett 0901632
*/
#include "Timer.h"
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

#define MAX_SIZE 50 // client_indexmum character size
#define NUMCONN 50 
#define MSG_CONNECTED	(0x41)	//A
#define MSG_SUCCESSFUL	(0x42)	//B

struct Cli_Info
{
	int socknum;
	FILE *mapfile;
	// Might want to put in the disconnect time here to store it
	Timer t;
	double duration;
};

std::vector<Cli_Info> Clients;

int recieve_file(int clientNum, char buff[], int bytes)
{
	fwrite(buff, sizeof(char), bytes, Clients.at(clientNum).mapfile);

	printf("Received:- %s \n", buff);
	return 1;
}
int main()
{
	int connections = 0;
	fd_set tempset, Masterset;  // descriptor set to be monitored
	struct sockaddr_in serv_addr, client_addr;
	char buff[MAX_SIZE];

	timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 500000;
	/* TODO: change this : */ for (int i = 0; i < NUMCONN; i++)Clients.push_back(Cli_Info());

	Clients.at(0).socknum = socket(AF_INET, SOCK_STREAM, 0);

	if(Clients.at(0).socknum < 0)
	{
		printf("Failed creating socket\n");
		return 0;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));


	serv_addr.sin_family = AF_INET; // IPv4 Address Family
	serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
	serv_addr.sin_port = htons(5099);


	if (bind(Clients.at(0).socknum, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Failed to bind on port\n");
		return 0;
	}

	if(listen(Clients.at(0).socknum, 5) == -1) // -1 is on error, 0 is successful
	{
		printf("Error: Unable to listen!\n");
		return 0;
	}

	// Clear file descriptor sets
	FD_ZERO(&tempset);
	FD_ZERO(&Masterset); // initialize the descriptor set to be monitored to empty
	FD_SET(Clients.at(0).socknum, &Masterset); // Add the listening socket in the MASTER set


	printf("Server setup - OK\n");
	printf("Server is now listening for connections!\n");
	while(true) // main server loop
	{
		// assign all currently monitored descriptor set to a local variable. This is needed because select
		// will overwrite this set and we will lose track of what we originally wanted to monitor.
		tempset = Masterset;

		// Look for socket activity
		select(FD_SETSIZE, &tempset, NULL, NULL, &timeout);


		if(FD_ISSET(Clients.at(0).socknum, &tempset)) // new client connection
		{
			int size = sizeof(client_addr);
			int Accepted, freeSock;
			Accepted = 0;

			// Add that new client's socket on to a std::vector
			// Sockets.push_back(accept(listen_desc, (struct sockaddr *)&client_addr, (socklen_t*)&size));
			for (int i = 1; i < NUMCONN; i++)
			{
				if (Clients.at(i).socknum == 0)
				{
					freeSock = i;
					Clients.at(freeSock).socknum = accept(Clients.at(0).socknum, (struct sockaddr *)&client_addr, (socklen_t*)&size);

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
				FD_SET(Clients.at(freeSock).socknum, &Masterset);
				// send (Sockets.at(SockNUM), buff, sizeof(buff), MSG_CONNECTED);
				// snprintf(buff, sizeof(buff), "%c", MSG_CONNECTED);
				// send(Sockets.at(SockNUM), buff, sizeof(buff), 0);

				// append socket number to this filename just to make sure no other client sending a smaller file will overwrite.
				Clients.at(freeSock).mapfile = fopen("tmp.json", "w");
				printf("Client's file is ready to be written\n");
				Clients.at(freeSock).t = Timer();
				Clients.at(freeSock).t.start();

				connections++;
				printf("Number of connected clients = %d \n\n", connections);
			}
		}

		// Process all connection sockets, start at 1 because the listening socket is at 0
		for (int i = 1; i < NUMCONN; i++)
		{
			if (Clients.at(i).socknum > 0)
			{
				if (FD_ISSET(Clients.at(i).socknum, &tempset))
				{

					// If the client i has connected and is sending byte data to the server, receive the data on that socket and write it to a file
					int bytes;

					while ((bytes = recv(Clients.at(i).socknum, buff, sizeof(buff), 0)) > 0)
					{
						recieve_file(i, buff, bytes);
					}

					fclose(Clients.at(i).mapfile);
					close(Clients.at(i).socknum);
					FD_CLR(Clients.at(i).socknum, &Masterset);
					Clients.at(i).socknum = 0;				
					Clients.at(i).duration = Clients.at(i).t.stop();

					printf("File took %5.6f seconds to complete upload\n", Clients.at(i).duration);
					printf("Client closed connection\n");
					connections--;
				}
			}
		}	
	}


	for (int i = 0; i < Clients.size(); i++) close(Clients.at(i).socknum); // Close all sockets
	return 0;
}