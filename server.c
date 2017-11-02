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
int channelSize = 0;

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
	int sockfd, server_port, loggedIn;
	server_port = atoi(argv[2]);
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int MAX_REQ_SIZE = sizeof(struct request_say); //should be 128
	char incoming_buff[MAX_REQ_SIZE];
	int recvlen; //size of message read
	socklen_t addrlen = sizeof(serv_addr);
	int req_type = -1;
	struct node* currentUserNode;

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
	//struct text_list t_list;
	//t_list.txt_type = TXT_LIST;
	//struct text_who t_who;
	//t_who.txt_type = TXT_WHO;
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

		//cast generic
		gen_request_struct = (struct request*) incoming_buff;

		//get type and cast to specific type
		//printf("\tincoming pac is type %d\n", gen_request_struct->req_type);
		//printf("\tfrom port: %d\n", serv_addr.sin_port);
		//printf("\tfrom IP: %d\n", serv_addr.sin_addr.s_addr);
		req_type = gen_request_struct->req_type;

		//set log in flag
		loggedIn = 1;
		if ((currentUserNode = find_user(dll_users, &serv_addr)) == NULL){
			loggedIn = 0;
		}

		switch(req_type){
			case REQ_LOGIN: {//login
				r_login = (struct request_login*) gen_request_struct;

				//take action - Add user to user list
				struct node* tempNode;
				tempNode = append(r_login->req_username, dll_users, &serv_addr);
				if(tempNode == NULL){
					displayData(dll_users);
					printf("error adding to user list\n");
				}

				printf("server: %s logs in\n", r_login->req_username);
				break;
			}
			case REQ_LOGOUT: {//logout
				if(!loggedIn){
					break;
				}

//				printf("\tbefore logout, channel looks like");
//				displayData(dll_channels);
//				printf("\tbefore logout, users looks like");
//				displayData(dll_users);

				r_logout = (struct request_logout*) gen_request_struct;
				//printf("pre remove\n");
				//take action - remove user from user list and every channel they are in. remove empty channels
				remove_user(dll_users, &serv_addr);

				//printf("post remove\n");

				//remove user from all channels
				struct node* channelNode;
				channelNode = dll_channels->next;
				while(channelNode != NULL){ //remove user if contained in channel
					printf("\tremoving user from each channel\n");
					//displayData(channelNode);
					remove_user(channelNode->inner, &serv_addr);

					//if channel has no users, remove channel
					if(channelNode->inner->next == NULL){
						printf("\tremoving empty channel named: %s\n", channelNode->data);
						remove_channel(channelNode->data, dll_channels);
					}
					channelNode = channelNode->next;
				}
//				printf("\tafter logout, channel list looks like");
//				displayData(dll_channels);
//				printf("\tafter logout, users looks like");
//				displayData(dll_users);


				break;
			}
			case REQ_JOIN: {//join
				r_join = (struct request_join*) gen_request_struct;
				struct node* tempNode;
				char tempBuff[USERNAME_MAX];

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//get user name of join request
				strcpy(tempBuff, currentUserNode->data);

				//if channel not created
				if((tempNode = find_channel(r_join->req_channel, dll_channels)) == NULL){
//					printf("\tchannel doesn't exist. creating\n");

					//create channel
					tempNode = append(r_join->req_channel, dll_channels, NULL);

					channelSize++;
				}

				//now channel exists
				//printf("channel exists\n");

				//finally channel is created, append user to it
				append(tempBuff, tempNode->inner, &serv_addr);

				printf("server: %s joins channel %s\n", tempBuff, tempNode->data);
				break;
			}
			case REQ_LEAVE: {//leave
				r_leave = (struct request_leave*) gen_request_struct;

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//take action - remove user from channel
				struct node* channelNode;
				channelNode = find_channel(r_leave->req_channel, dll_channels);
				printf("server: %s leaves channel %s\n", currentUserNode->data, r_leave->req_channel);

				//if channel doesn't exist
				if(channelNode == NULL){
					//No channel by the name
					strcpy(t_error.txt_error, "No channel by the name ");
					strcat(t_error.txt_error, r_leave->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				}

				//if channel has no users, remove channel
				if(channelNode->inner->next != NULL){
					printf("server: removing empty channel %s\n", channelNode->data);
					remove_channel(channelNode->data, dll_channels);
				}

				break;
			}
			case REQ_SAY: {//say
				r_say = (struct request_say*) gen_request_struct;

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//take action -


				printf("\tchannels look like: ");
				displayData(dll_channels->next);
				printf("\tusers look like: ");
				displayData(dll_users->next);
				break;
			}
			case REQ_LIST: {//list
				r_list = (struct request_list*) gen_request_struct;

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//take action - send a list all channels
				struct node* currentNode = dll_channels->next;
				int structSize = sizeof(struct text_list) + (channelSize * sizeof(struct channel_info));
				struct text_list* t_list = (struct text_list*)malloc(structSize);
				if (t_list == NULL){
					fprintf(stderr,"failed malloc\n");
					continue;
				}

				t_list->txt_type = TXT_LIST;
				t_list->txt_nchannels = channelSize;

				int i = 0;
				while(currentNode != NULL){
					//printf("cur channel data is %s\n", currentNode->data);
					strcpy(t_list->txt_channels[i].ch_channel, currentNode->data);
					//printf("ith item is %s\n", t_list->txt_channels[i].ch_channel);
					currentNode = currentNode->next;
					i++;
				}

				//send to whoever just asked for list
				sendto(sockfd, t_list, structSize, 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				free(t_list);

				break;
			}
			case REQ_WHO:{ //who
				r_who = (struct request_who*) gen_request_struct;

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				struct node* channelNode;
				struct node* userNode;

				channelNode = find_channel(r_who->req_channel, dll_channels);
				if(channelNode == NULL){
					printf("server: %s trying to list users in non-existing channel %s\n", currentUserNode->data, r_who->req_channel);
					strcpy(t_error.txt_error, "No channel by the name ");
					strcat(t_error.txt_error, r_who->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				int numInChannel = channelNode->inner->usersInChannel;
				printf("\tnum user in %s is %d\n", channelNode->data, numInChannel);

				//take action - send channel name and list of every user in that channel
				int structSize = sizeof(struct text_who) + (numInChannel * sizeof(struct user_info));
				struct text_who* t_who = (struct text_who*)malloc(structSize);
				if (t_who == NULL){
					fprintf(stderr,"failed malloc\n");
					continue;
				}

				t_who->txt_type = TXT_WHO;
				strcpy(t_who->txt_channel, r_who->req_channel);
				t_who->txt_nusernames =numInChannel;

				int i = 0;
				userNode = channelNode->inner->next;
				while(userNode != NULL){
					//printf("cur channel data is %s\n", currentNode->data);
					strcpy(t_who->txt_users[i].us_username, userNode->data);
					//printf("ith item is %s\n", t_list->txt_channels[i].ch_channel);
					userNode = userNode->next;
					i++;
				}

				//send to whoever just asked for list
				sendto(sockfd, t_who, structSize, 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				printf("\tfreeing\n");
				free(t_who);
				break;
			}
			default:{
				strcpy(t_error.txt_error, "Server couldn't match message type.");
				sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				fprintf(stderr, "ERROR - server: no matching req type\n");
			}
		}

		//???

	}
	printf("server exiting\n");
	removeAll(dll_channels);
	removeAll(dll_users);
	return 0;
}
