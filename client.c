/*
 * client.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 *      Credit:
 *      	help from: http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html
 */
#include <stdio.h>
#include "duckchat.h"
#include "raw.h"
#include <sys/socket.h>
#include <sys/types.h> // ???
#include <netinet/in.h> // AF_INET and AF_INET6 address families
#include <sys/un.h>  // AF_UNIX address family. Used for local communication between programs running on the same computer
#include <arpa/inet.h> // Functions for manipulating numeric IP addresses.
#include <netdb.h> // Functions for translating protocol names and host names into numeric addresses.
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int CLIENT_PORT = 4000;

/*
 * Spec:
 *
 * parse 3 command line arguments. #look at previous OS proj. argv argc
 *
 * connect to chat server
 *
 * on connection, join channel common. #send join message
 *
 * prompt user. #literally prompt or wait for input?
 *
 * on 'Enter' key press, send user text to server*. #use 'say request' message.
 *
 * * special command list = ...
 *
 * keep track of current channel. send command, unless command is switch.
 *
 * on switch, check if already joined. error if no.
 *
 * on leave channel, discard text until join channel
 *
 * on receive text, save and remove('\b') current typed values, display '[channel][username]: text', replace saved text
 *
 *
 *
 */

int main(int argc, char *argv[]){
	//set raw mode because it says so
	raw_mode();
	atexit(cooked_mode);

	//parse 3 command line arguments. server_socket(address), server_port, username
	//argc is num args. argv is list of args

	if (argc != 4){
		printf("Usage: ./client server_socket server_port username\n");
		return 1;
	}

	//create socket
	struct sockaddr_in serv_addr;
	struct sockaddr_in client_addr;
	struct hostent *server;
	struct hostent *client;
	int sockfd, server_port;
	char username[25]; 

	server_port = atoi(argv[2]);
	strncpy(username, argv[3], strlen(argv[3]));

	if (( sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stderr, "ERROR - client: can’t open stream socket\n");
		return 1;
	}
	if ((server = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "ERROR - client: no such host\n");
		return 1;
	}

	if ((client = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "ERROR - client: no such host\n");
		return 1;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	bzero((char *)&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	bcopy((char *)client->h_addr, (char *)&client_addr.sin_addr.s_addr, client->h_length);
	client_addr.sin_port = htons(CLIENT_PORT);

	//bind socket to client port number
	if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
		fprintf(stderr, "ERROR - client: bind failed\n");
		return 1;
	}

	/* send request to server, receive reply */
	char *buf = "The Cheese is in The Toaster";
	sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr)); //TODO sends garbage

	//connect to chat server
	char current_char;
	int active = 1;
	int input_buff_ctr = 0;
	int n_flag = 0;
	int s_flag = 0;
	char input_buff[128];
	input_buff[0] = '\0';

	printf("> ");
	while(active){

		//if server message waiting to be read in (SELECT),
		if(0){
			s_flag = 1;

			//read server message

			//take action
		}

		//read and print the next char in stdin
		current_char = (char)fgetc(stdin);
		printf("%c", current_char);

		if (current_char == '\n'){
			n_flag = 1;
			if (strcmp(input_buff, "/exit") == 0){
				active = 0;
			}
			if (strcmp(input_buff, "/switch") == 0){
				//handle channel switch
			}
			//if --handle other commands--

			else{ //not a command, must be a message to be sent
				//make message from input_buffer

				//send message

				//clear input_buffer
				input_buff_ctr = 0;
				input_buff[input_buff_ctr+1] = '\0';
			}
		}
		else{
			input_buff[input_buff_ctr++] = current_char;
			input_buff[input_buff_ctr] = '\0';
		}

		//if we just typed \n and there are no messages for us from the server
		if(n_flag && !s_flag){
			printf("> ");
		}
		n_flag=0;
		s_flag=0;
	}


	//set cooked before return
	printf("client closing\n");
	return 0;
}


