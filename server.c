/*
 * server.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 *      Credit: Conceptual help from classmates: Anisha, Amie and Kaley
 */
//#include "listOfLists.h"
#include <stdio.h>
#include "duckchat.h"
#include "raw.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> // AF_INET and AF_INET6 address families
#include <sys/un.h>  // AF_UNIX address family. Used for local communication between programs running on the same computer
#include <arpa/inet.h> // Functions for manipulating numeric IP addresses.
#include <netdb.h> // Functions for translating protocol names and host names into numeric addresses.
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "listOfLists.h"

/*
 * Spec:
 * read 2 command line arg
 *
 * output logging info when server takes action.
 * on receive message, print [channel][user][message]
 *
 * on say request send [message] to all [user] in [channel]
 *
 * keep track of each channel and all users in each channel
 *
 * when channel has no users, delete. when users joins void channel, create.
 *
 * ignore message from non logged in users
 */

struct node* dll_channels;
struct node* dll_users;

void handleRead(int readResult){
	if (readResult < 0){
		printf("ERROR reading from socket\n");
		exit(0);
	}
}

int main(int argc, char *argv[]){
	raw_mode(); //set raw
	atexit(cooked_mode); //return to cooked on normal exit

	if (argc != 3){
		printf("Usage: ./server domain_name port_num\n");
		return 1;
	}
	//create server and user lists
	dll_channels = initDLL();
	dll_users = initDLL();

	//create socket
	int sockfd, server_port;
	server_port = atoi(argv[2]);
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int MAX_REQ_SIZE = sizeof(struct request_say); //should be 128
	char incoming_buff[MAX_REQ_SIZE];
	int recvlen; //size of message read
	socklen_t addrlen = sizeof(serv_addr);
	int req_type = -1;

	//statically allocate request structs
	struct request* gen_request_struct;
	struct request_login* r_login = NULL;
	struct request_logout* r_logout = NULL;
	struct request_join* r_join = NULL;
	struct request_leave* r_leave = NULL;
	struct request_say* r_say = NULL;
	struct request_list* r_list = NULL;
	struct request_who* r_who = NULL;

	//statically allocate text structs
	struct text_say t_say;
	t_say.txt_type = TXT_SAY;
	struct text_list t_list;
	t_list.txt_type = TXT_LIST;
	struct text_who t_who;
	t_who.txt_type = TXT_WHO;
	struct text_error t_error;
	t_error.txt_type = TXT_ERROR;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			fprintf(stderr, "ERROR - server: can’t open stream socket\n");
			return 1;
	}
	if ((server = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "ERROR - server: no such host\n");
		return 1;
	}
	//bind sockfd
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		fprintf(stderr, "ERROR - server: can't bind socket\n");
		return 1;
	}

	//while true
	while(1){
		//recvfrom sockfd
		recvlen = recvfrom(sockfd, incoming_buff, MAX_REQ_SIZE, 0, (struct sockaddr *)&serv_addr, &addrlen);
		//printf("Something came in\n");

		//cast generic
		gen_request_struct = (struct request*) incoming_buff;

		//get type and cast to specific type
		//printf("type is %d\n", gen_request_struct->req_type);
		req_type = gen_request_struct->req_type;
		switch(req_type){
			case 0:
				r_login = (struct request_login*) gen_request_struct;

				//take action - Add user to user list
				append(r_login->req_username, dll_users, &serv_addr);

				printf("server: %s logs in\n", r_login->req_username);
				break;
			case 1:
				r_logout = (struct request_logout*) gen_request_struct;

				//take action - remove user from user list and every channel they are in


				break;
			case 2:
				r_join = (struct request_join*) gen_request_struct;
				struct node* tempNode;
				char tempBuff[USERNAME_MAX];

				//if user not logged in
				if ((tempNode = find_user(dll_users, &serv_addr)) == NULL){
					//send error
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					//printf("user not logged in\n");
					break;
				}

				//get user name of join request
				strcpy(tempBuff, tempNode->data);

				//if channel not created
				if((tempNode = find_channel(r_join->req_channel, dll_channels)) == NULL){
					//printf("channel doesn't exist. creating\n");

					//create channel
					tempNode = append(r_join->req_channel, dll_channels, NULL);
				}
				else{
					//printf("channel does exist\n");
				}
				// otherwise channel is created, append user to it
				append(tempBuff, tempNode->inner, &serv_addr);

				printf("server: %s joins channel %s\n", tempBuff, tempNode->data);
				break;
			case 3:
				r_leave = (struct request_leave*) gen_request_struct;
				//take action -

				break;
			case 4:
				r_say = (struct request_say*) gen_request_struct;
				//take action -

				break;
			case 5:
				r_list = (struct request_list*) gen_request_struct;
				//take action -

				break;
			case 6:
				r_say = (struct request_say*) gen_request_struct;
				//take action -

				break;
			default:
				fprintf(stderr, "ERROR - server: no matching req type\n");
		}

		//???

	}
	printf("server exiting\n");
	removeAll(dll_channels);
	removeAll(dll_users);
	return 0;
}
