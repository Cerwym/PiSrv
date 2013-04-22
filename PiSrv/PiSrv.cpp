/*
PiSrv, a small TCP server for the raspberry Pi - Peter Lockett 0901632
*/
#include "Timer.h"
#include <boost/filesystem/operations.hpp> // includes boost/filesystem/path.hpp
#include <boost/filesystem/fstream.hpp>    // ditto
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

int errorCount = 0;

enum CliType{OSX = 0, ANDROID = 1};

enum CliMode{UPLOAD = 0, DOWNLOAD = 1, FINISHED = 2};

struct Cli_Info
{
	int socknum; // The socket a client gets accept('d on.
	FILE *mapfile; // The file the server opens for the client on connection then closes on a disconnect
	Timer t; // Time used for tracking how long an upload took
	CliType ctype; // OSX or Android Information
	CliMode mode; // The mode the client connects on.
	double duration; // The total time took.
	int bytes_to_read;
	char *uid;
	bool hasError;
};

std::vector<Cli_Info> Clients;

int parseInt(char *ch)
{
	int temp=0,neg=0;
	while(*ch!=NULL)
	{
		if(temp==0&&*ch=='-')neg=1;
		if(*ch>='0'&&*ch<='9')
		{
			if(temp==0)
			{
				temp=*ch-'0';
			}
			else if(temp == 12)
				break; // Hit the \n from android
			else
			{
				temp*=10;
				temp+=*ch-'0';
			}
		}
		++ch;
	}
	if(neg==1)temp*=-1;
	return temp;
}
int recieve_file(int clientNum, char buff[], int bytes)
{
	fwrite(buff, sizeof(char), bytes, Clients.at(clientNum).mapfile);

	return 1;
}

int rename_file(Cli_Info cli)
{
	// Will need to parse object as json file, get the "filename" string and pass it in as an arg.
	Json::Value root;
	Json::Value vec(Json::arrayValue);
	Json::Reader reader;
	std::ifstream test(cli.uid, std::ifstream::binary);

	bool Parsed = reader.parse(test, root, false);
	if(!Parsed){ // Throw out an error to say that the document has not been parsed correctly.
			std::cout << reader.getFormatedErrorMessages() << "\n";
			errorCount++;
			remove(cli.uid);
			return 0;}

	vec.append(Json::Value(1));

	if (cli.ctype == OSX)
		root["Time from OSX"] = cli.duration;
	else if (cli.ctype == ANDROID)
		root["Time from ANDROID"] = cli.duration;

	std::string filename = root.get("filename", "tmp.json").asString();
	std::string ext = root.get("extension", "tmp.json").asString();

	std::ofstream f;
	f.open(cli.uid);
	f << root.toStyledString();
	f.close();
	
	bool success = false;

	char str[30];
	int count = 1;
	sprintf(str, "%s.%s", filename.c_str(), ext.c_str());
	std::string fname = std::string(str);
	if (boost::filesystem::exists(fname.c_str()))
	{
		printf("File Exists\n");
		do
		{
			sprintf(str, "%s - %d.%s", filename.c_str(),count, ext.c_str());
			fname = std::string(str);
			if (boost::filesystem::exists(fname.c_str()))
				count++; // File was found, increment counter
			else
			{
				rename(cli.uid, str);
				success = true;
			}
		}while (success == false);
	}
	else
		rename(cli.uid, str);
	return 1;
}

int send_file(int sock)
{
	printf("Sending file to client\n");
	// toodo, android messages must be terminated with a \n
	std::ifstream f("Demo.pjl", std::ifstream::binary);
	if(f)
	{
		f.seekg(0, f.end);
		int length = f.tellg();
		f.seekg(0, f.beg);

		char * buffer = new char [length];

		std::cout << "Reading " << length << " characters... ";
		// read data as a block:
		f.read(buffer,length);

		f.close();

		// Send handshake to the client to notify it how many bytes the incoming file is
		// send(sock, length, length
		send(sock, buffer, length, 0);

		delete[] buffer;
		// send(sock,"\n", sizeof(char), 0);
	}
}


int main()
{
	int connections = 0;
	int c_id = 0;
	int n;
	fd_set tempset, Masterset;  // descriptor set to be monitored
	struct sockaddr_in serv_addr, client_addr;
	char buff[800];
	char androidACK[] = {'a', '\n'};

	timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 500000;

	//Beej states that using PF_INET in the call to socket and AF_INET elsewhere is the correct choice, in socket.h however they appear to be the same
	int s_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(s_socket < 0)
	{
		printf("Failed creating socket\n");
		return 0;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET; // IPv4 Address Family
	serv_addr.sin_addr.s_addr = htons(INADDR_ANY);
	serv_addr.sin_port = htons(5099); // Application will be communicating on TCP port 5099

	n = 1;
	if (setsockopt(s_socket, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n) < 0)
	{
		printf("Unable to set SO_REUSEADDR\n");
		return 0;
	}

	if (bind(s_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Failed to bind on port\n");
		return 0;
	}

	if(listen(s_socket, 5) == -1) // -1 is on error, 0 is successful
	{
		printf("Error: Unable to listen!\n");
		return 0;
	}

	// Clear file descriptor sets
	FD_ZERO(&tempset);
	FD_ZERO(&Masterset); // initialize the descriptor set to be monitored to empty
	FD_SET(s_socket, &Masterset); // Add the listening socket in the MASTER set

	printf("Server setup - OK\n");
	printf("Server is now listening for connections!\n");
	while(true) // main server loop
	{
		// assign all currently monitored descriptor set to a local variable. This is needed because select
		// will overwrite this set and we will lose track of what we originally wanted to monitor.
		tempset = Masterset;

		// Look for socket activity
		select(FD_SETSIZE, &tempset, NULL, NULL, &timeout);

		if(FD_ISSET(s_socket, &tempset)) // new client connection
		{
			int size = sizeof(client_addr);
			int Accepted, freeSock;
			c_id++;
			Accepted = 0;
			Cli_Info tClient;
			tClient.uid = new char[10];
			sprintf(tClient.uid, "%d.tmp", c_id);

			tClient.socknum = accept(s_socket, (struct sockaddr *)&client_addr, (socklen_t*)&size);

			printf("Accepted new client on socket %d\n", tClient.socknum);
			// Put the transmission socket in the master set
			FD_SET(tClient.socknum, &Masterset);


			char recD;
			recv(tClient.socknum, &recD, 1, 0);
			if (recD == 'o')
			{
				printf("Client is osx\n");
				tClient.ctype = OSX;
			}

			else if (recD == 'a')
			{
				printf("Client is android\n");
				tClient.ctype = ANDROID;
				//send(tClient.socknum, androidACK, sizeof(androidACK), 0);
			}

			// the next character sent should be either a 'u' or 'd', placed in *s
			recv(tClient.socknum, &recD, 1, 0);
			if (recD == 'u')
			{
				tClient.mode = UPLOAD;
				printf("Client has connected in upload mode\n");
				// if the client is connecting on an android device, check how many bytes it wants to send
				if (tClient.ctype == ANDROID)
				{
					// Read until we hit a newline.
					const int SIZE = 20;
					char line[SIZE + 1];
					int count = 0;
					while (count < SIZE) {
						recv(tClient.socknum, &line[count], 1, 0);
						if (line[count] == '\n') break;
						++count;
					}
					line[count] = '\0';
					printf("Read %d = [%s]\n", count, line);

					int bytes_to_read = parseInt(line);
					printf("Client is wanting to save a total of %d bytes\n", bytes_to_read);
					tClient.bytes_to_read = bytes_to_read;
				}
			}
			else if (recD == 'd')
			{
				tClient.mode = DOWNLOAD;
				printf("Client has connected in download mode\n");
			}
		
			// Add that new client's socket on to a std::vector
			Clients.push_back(tClient);
			connections++;
			//c_id++;
			printf("Number of connected clients = %d \n\n", connections);
		}

		// Process all connection sockets, start at 1 because the listening socket is at 0
		for (int i = 0; i < Clients.size(); i++)
		{
			if (Clients.at(i).socknum > 0)
			{
				if (FD_ISSET(Clients.at(i).socknum, &tempset))
				{
					if (Clients.at(i).mode == DOWNLOAD)
					{
						send_file(Clients.at(i).socknum);
						Clients.at(i).mode = FINISHED;
					}
					if (Clients.at(i).mode == UPLOAD)
					{
						//int bytes_to_read = recv(Clients.at(i).socknum, NULL, sizeof(int), 0);
						//printf("Ready to read in %d bytes\n", bytes_to_read);
						Clients.at(i).mapfile = fopen(Clients.at(i).uid, "w");
						printf("Client's file is ready to be written\n");
						Clients.at(i).t = Timer();
						Clients.at(i).t.start();
						
						// If the client i has connected and is sending byte data to the server, receive the data on that socket and write it to a file
						int bytes_read = 0;
						int bytes_in = 1;
						while ( (bytes_read != Clients.at(i).bytes_to_read) && bytes_in != 0) // FIX this to BYTES_TO_READ
						{
							bytes_in = recv(Clients.at(i).socknum, buff, sizeof(buff), 0);
							if (bytes_in !=0)
							{
								printf("Read %d. Buffer contains: [", bytes_in);
								fwrite(buff, 1, bytes_in, stdout);
								printf("]\n");
								printf("Read in %d bytes...\n", bytes_in);
								recieve_file(i, buff, bytes_in);
								bytes_read += bytes_in;
							}
						}
					
						// Duration is calculated AFTER the file has been received and temporary file is closed for 'fairness'
						Clients.at(i).duration = Clients.at(i).t.stop();
						fclose(Clients.at(i).mapfile);
						printf("File took %5.6f seconds to complete upload\n", Clients.at(i).duration);
						Clients.at(i).mode = FINISHED;

					}
					if (Clients.at(i).mode == FINISHED)
					{
						//send(Clients.at(i).socknum, "f", sizeof(char), 0);
						close(Clients.at(i).socknum);
						FD_CLR(Clients.at(i).socknum, &Masterset);
						Clients.at(i).socknum = 0;				

						printf("Client closed connection\n");

						if (rename_file(Clients.at(i)) == 1)
							printf("File was renamed successfully\n");
						else
							printf("File was not saved\n");

						// Print out the amount of errors there currently is
						printf("There have been %d errors in parsing objects\n", errorCount);
						Clients.erase(Clients.begin() + i);
						connections--;
					}
				}
			}
		}	
	}


	for (int i = 0; i < Clients.size(); i++) close(Clients.at(i).socknum); // Close all sockets
	return 0;
}