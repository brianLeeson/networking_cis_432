/*
 * client.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 *      Credit:
 *      	help from: http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html
 *      	This implementation of a DLL is a modified form of the DLL found at:
 *      	https://www.tutorialspoint.com/data_structures_algorithms/doubly_linked_list_program_in_c.htm
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
	raw_mode(); //set raw
	atexit(cooked_mode); //return to cooked on normal exit

	//parse 3 command line arguments. server_socket(address), server_port, username
	//argc is num args. argv is list of args

	if (argc != 4){
		printf("Usage: ./client server_socket server_port username\n");
		return 1;
	}

	//create socket
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int sockfd, server_port;
	char username[25]; 

	server_port = atoi(argv[2]);
	strcpy(username, argv[3]);

	if (( sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stderr, "ERROR - client: can’t open stream socket\n");
		return 1;
	}
	if ((server = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "ERROR - client: no such host\n");
		return 1;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	//Statically allocate request structs
	struct request_login req_login;
	struct request_logout  req_logout;
	struct request_join req_join;
	struct request_leave req_leave;
	struct request_say req_say;
	struct request_list req_list;
	struct request_who req_who;

	//init to chat server - login, join common
	//login
	req_login.req_type = REQ_LOGIN;
	strcpy(req_login.req_username, username);
	sendto(sockfd, &req_login, sizeof(struct request_login), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr)); //TODO sends garbage

	//join common
	req_join.req_type = REQ_JOIN;
	strcpy(req_join.req_channel, "Common");
	sendto(sockfd, &req_join, sizeof(struct request_join), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

	//while logged in, handle user input, handle server messages
		char current_char;
		char cur_channel[32];
		int logged_in = 1;
		int input_buff_ctr = 0;
		int n_flag = 0;
		int s_flag = 0;
		char input_buff[128];
		input_buff[0] = '\0';

		printf("> ");
		while(logged_in){

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
				if (!strcmp(input_buff, "/exit")){
					logged_in = 0;
				}
				else if (!strcmp(input_buff, "/logout")){
					sendto(sockfd, &req_logout, sizeof(struct request_logout), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					logged_in = 0;
				}
				else if (!strncmp(input_buff, "/join", 5 * sizeof(char))){
					//parse channel name from input_buff
					strcpy(req_join.req_channel, input_buff+6);
					sendto(sockfd, &req_join, sizeof(struct request_join), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

					//add to channel list
					//insertLast(input_buff+6);

					//clear input_buffer
					input_buff_ctr = 0;
					input_buff[input_buff_ctr+1] = '\0';
				}
				else if (!strncmp(input_buff, "/leave", 6 * sizeof(char))){
						//parse channel name from input_buff
						strcpy(req_leave.req_channel, input_buff+7);
						sendto(sockfd, &req_leave, sizeof(struct request_leave), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
						//clear input_buffer
						input_buff_ctr = 0;
						input_buff[input_buff_ctr+1] = '\0';
				}
				else if (!strcmp(input_buff, "/list")){
					sendto(sockfd, &req_list, sizeof(struct request_list), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					//clear input_buffer
					input_buff_ctr = 0;
					input_buff[input_buff_ctr+1] = '\0';
				}
				else if (!strncmp(input_buff, "/who", 4 * sizeof(char))){
					//parse channel name from input_buff
					printf("%s\n",input_buff+5);
					strcpy(req_who.req_channel, input_buff+5);
					sendto(sockfd, &req_who, sizeof(struct request_who), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					//clear input_buffer
					input_buff_ctr = 0;
					input_buff[input_buff_ctr+1] = '\0';
				}
				else if (!strncmp(input_buff, "/switch", 7 * sizeof(char))){
					//if client is a member of the specified channel
						//set active channel to be that channel

					//else
						//display error and don't move channel
						printf("%s", "You have not subscribed to channel ");
						printf("%s\n", input_buff+8);

					//clear input_buffer
					input_buff_ctr = 0;
					input_buff[input_buff_ctr+1] = '\0';
				}
				else if(!strncmp(input_buff, "/", sizeof(char))){
					printf("%s", "*Unknown command\n");
				}

				//if --handle other commands--

				else{ //not a command, must be a message to be sent
					//printf("%s", "must send message\n");
					req_say.req_type = REQ_SAY;

					//get current channel from list of channels //TODO

					strcpy(cur_channel, "Common");

					//make message from input_buffer
					strcpy(req_say.req_channel, cur_channel);
					strcpy(req_say.req_text, input_buff);

					//send message
					sendto(sockfd, &req_say, sizeof(struct request_say), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

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


