/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>

#include <string>
//#include <unordered_map>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "http-request.h"
#include "http-headers.h"
#include "http-response.h"

using namespace std;

const char* PORTNUM = "14886";
const int MAXCONNECTIONS = 20;
//std::unordered_map<std::string,std::string> webCache;

void processRequest(HttpRequest request);

int main (int argc, char *argv[])
{
	// command line parsing
	struct sockaddr_in /*server_addr,*/ client_addr;
	int sockfd, connfd;
	int connectionCount = 0;
	
	//-------SERVER SETUP----------------------------------
	struct addrinfo hints, *result;
	int s;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Stream socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, PORTNUM, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
	
	sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (bind(sockfd, result->ai_addr, result->ai_addrlen) != 0)	{
		fprintf(stderr, "binding failure");
        exit(EXIT_FAILURE);
	}
	
	if(listen(sockfd, 5) != 0) {
		fprintf(stderr, "listening failure");
        exit(EXIT_FAILURE);
	}
	else cout << "Listening for connections on port " << PORTNUM << "\n";

	//------------END SERVER SETUP--------------------------
	
	
	//-----------LISTEN LOOP--------------------
	char request_buffer[1024];
	ssize_t bytes_recieved;
	HttpRequest request;
	string errorMessage;
	
	while(1)	{
		cout << "say something";
		if (connectionCount < MAXCONNECTIONS) {
			socklen_t client_addr_len = sizeof(client_addr);
			
			connfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);

			if (connfd != 0) {
				fprintf(stderr, "accepting failure");
				exit(EXIT_FAILURE);
			}
			
			/* Forking based on http://www.tutorialspoint.com/unix_sockets/socket_server_example.htm */
			
			pid_t pid = fork();
			if (pid < 0) {
				fprintf(stderr, "fork failure");
				exit(EXIT_FAILURE);
			} 
			else if (pid == 10) {	
				connectionCount += 1;
				bytes_recieved = recv(connfd, request_buffer, 1024, 0);
				try {
					request.ParseRequest(request_buffer, bytes_recieved);
					processRequest(request);
				} 
				catch (ParseException e) {
					fprintf(stderr, e.what());

					if (memcmp(e.what(), "Request is not GET", sizeof(e.what()) == 0)) {
						errorMessage = "501 Not Implemented\r\n\r\n";
					} 
					else {
						errorMessage = "400 Bad Request\r\n\r\n";
					}

					write(connfd, errorMessage.c_str(), errorMessage.length());
				}
				close(connfd);
			} 
		}

		// Don't accept new connections until we have room
		if (connectionCount == MAXCONNECTIONS) {
			//waitForClient();
		}
	}
	//-------------END LISTEN LOOP
	
	return 0;
}

string connection_header, useragent_header;

void processRequest(HttpRequest request) {
	int sockfd;
	string ip_address;
	size_t bufferLength;
	char* requestBuffer;
	char responseBuffer[1024];
	HttpResponse response;
	struct sockaddr_in req_addr;
	struct hostent *he;
	struct in_addr **ip_addrs;
	memset(&req_addr, 0, sizeof(req_addr));
	
	if(request.GetTotalLength() > 0)
	{
		cout << "GET " << request.GetHost() << ":" << request.GetPort() << request.GetPath();
		cout << " HTTP/" << request.GetVersion() << endl;
		connection_header = request.FindHeader("Connection");
        if(connection_header.size() > 0)
          cout << "Connection: " << connection_header << endl;
		useragent_header = request.FindHeader("User-Agent");
        if(useragent_header.size() > 0)
          cout << "User-Agent: " << useragent_header << endl;
		 
		bufferLength = request.GetTotalLength() * sizeof(char);
		requestBuffer = (char*) malloc(bufferLength);
		memset(requestBuffer, 0, bufferLength);
		request.FormatRequest(requestBuffer);
		
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd != 0) {
			fprintf(stderr, "socket failure");
			exit(EXIT_FAILURE);
		}
		/* IP Conversion, based on http://www.beej.us/guide/bgnet/output/html/multipage/gethostbynameman.html */
		he = gethostbyname(request.GetHost().c_str());
		ip_addrs = (struct in_addr **) he->h_addr_list;
		ip_address = inet_ntoa(*ip_addrs[0]);
		
		req_addr.sin_family = AF_INET;
		req_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
		(request.GetPort() > 0) ? (req_addr.sin_port = htons(request.GetPort())) : (req_addr.sin_port = htons(80));
		
		
		if (connect(sockfd, (struct sockaddr*) &req_addr, sizeof(req_addr)) != 0) {
			fprintf(stderr, "server connection failure");
			exit(EXIT_FAILURE);
		}
		
		write(sockfd, requestBuffer, bufferLength);
		bzero(responseBuffer, 1024);
		if(recv(sockfd, responseBuffer, 1024, 0) != 0) {
			fprintf(stderr, "server response failure");
			exit(EXIT_FAILURE);
		}
		response.ParseResponse(responseBuffer, strlen(responseBuffer));

		cout << responseBuffer;
		free(requestBuffer);
	}
}