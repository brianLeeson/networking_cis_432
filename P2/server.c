/*
 * server.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 *      Credit:
 *      Conceptual help from classmates: randy, jack, amie, anisha, kaley and alision
 *      This implementation of a DLL is a *heavily* modified form of the DLL found at:
 *     	https://www.tutorialspoint.com/data_structures_algorithms/doubly_linked_list_program_in_c.htm
 */

#include <stdio.h>
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
#include <uuid/uuid.h>
#include "listOfLists.h"
#include "duckchat.h"
#include "raw.h"

#define ADDRESS_MAX 64
#define ID_MAX 1000

struct node* dll_channels;
struct node* dll_users;
struct node* dll_adjacency;

struct node* serv_self;


long long SAY_IDS[ID_MAX];
int CUR_ID_POS = 0;

void handleRead(int readResult){
	if (readResult < 0){
		printf("ERROR reading from socket\n");
		exit(0);
	}
}

//TODO Add support for sending Leave when a Say cannot be forwarded.

int main(int argc, char *argv[]){
	raw_mode(); //set raw
	atexit(cooked_mode); //return to cooked on normal exit

	if (argc < 3){
		printf("Usage: ./server domain_name port_num ...\n");
		return 1;
	}
	if ((argc % 2) != 1){
		printf("Error - Pairs of hosts and ports incomplete");
	}

	//create server and user lists
	dll_channels = createNode();
	dll_users = createNode();
	dll_adjacency = createNode();

	//populate server adjacency list
	int i;
	int adj_server_port;
	char serverName[ADDRESS_MAX];
	struct sockaddr_in adj_serv_addr;
	struct hostent *adj_server_address;
	for (i = 1; i < argc; i=i+2){

		printf("in for loop %d. argc is %d\n", i, argc);
		//create server name
		printf("argv pair is %s and %s\n", argv[i], argv[i+1]);
		strcat(serverName, argv[i]);
		strcat(serverName, argv[i+1]);
		printf("finished concat\n");

		//create server sockaddr_in
		adj_server_port = atoi(argv[i+1]);
		printf("byte order port %d\n", htons(adj_server_port));
		if ((adj_server_address = gethostbyname(argv[i])) == NULL) {
			fprintf(stderr, "ERROR - server: no such host\n");
			return 1;
		}

		//create serv_addr
		bzero((char *)&adj_serv_addr, sizeof(adj_serv_addr));
		adj_serv_addr.sin_family = AF_INET;
		bcopy((char *)adj_server_address->h_addr, (char *)&adj_serv_addr.sin_addr.s_addr, adj_server_address->h_length);
		adj_serv_addr.sin_port = htons(adj_server_port);

		printf("adj_serv_addr port: %d, host: %d\n", adj_serv_addr.sin_port, adj_serv_addr.sin_addr.s_addr);

		append(serverName, dll_adjacency, &adj_serv_addr);
		serverName[0] ='\0';
	}
	serv_self = dll_adjacency->next;
	printf("adjacency list created\n");

	//create socket
	int sockfd, server_port, loggedIn;
	server_port = atoi(argv[2]);
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int MAX_REQ_SIZE = sizeof(struct request_say); //should be 128
	char incoming_buff[MAX_REQ_SIZE];
	socklen_t addrlen = sizeof(serv_addr);
	int req_type = -1;
	struct node* currentUserNode;

	//statically allocate request structs
	struct request* gen_request_struct;
	struct request_login* r_login = NULL;
	struct request_join* r_join = NULL;
	struct request_leave* r_leave = NULL;
	struct request_say* r_say = NULL;
	struct request_who* r_who = NULL;

	//statically allocate text structs
	struct text_say t_say;
	t_say.txt_type = TXT_SAY;
	struct text_error t_error;
	t_error.txt_type = TXT_ERROR;

	//statically allocate serv structs
	struct serv_join* s_join;
	struct serv_leave* s_leave;
	struct serv_say* s_say;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			fprintf(stderr, "ERROR - server: can’t open stream socket\n");
			return 1;
	}
	if ((server = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "ERROR - server: no such host\n");
		return 1;
	}
	//create serv_addr
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	//bind sockfd
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		fprintf(stderr, "ERROR - server: can't bind socket\n");
		return 1;
	}

	//while true
	while(1){
		//recvfrom sockfd
		recvfrom(sockfd, incoming_buff, MAX_REQ_SIZE, 0, (struct sockaddr *)&serv_addr, &addrlen);
		printf("recieved message!\n");

		//cast generic
		gen_request_struct = (struct request*) incoming_buff;

		//get type and cast to specific type
		req_type = gen_request_struct->req_type;

		//set login flag
		loggedIn = 1;
		if ((currentUserNode = find_user(dll_users, &serv_addr)) == NULL){
			loggedIn = 0;
		}

		switch(req_type){
			case REQ_LOGIN: { //login
				r_login = (struct request_login*) gen_request_struct;

				//take action - Add user to user list
				struct node* tempNode;
				tempNode = append(r_login->req_username, dll_users, &serv_addr);
				if(tempNode == NULL){
					printf("server: error adding to user list\n");
					break;
				}

				printf("server: %s logs in\n", r_login->req_username);
				dll_users->numNodesInList++;
				break;
			}
			case REQ_LOGOUT: { //logout
				if(!loggedIn){
					break;
				}
				printf("server: %s logs out\n", currentUserNode->data);

				//take action - remove user from user list and every channel they are in. remove empty channels
				remove_user(dll_users, &serv_addr);

				//remove user from all channels
				struct node* channelNode;
				channelNode = dll_channels->next;
				while(channelNode != NULL){
					//remove user if contained in channel
					remove_user(channelNode->inner, &serv_addr);

					//if channel has no users AND channel has at most 1 serv in adj list
					if((channelNode->inner->next == NULL) && (channelNode->adj_list->numNodesInList <= 1)){
						printf("need to send leave message\n");
						displayData(channelNode->adj_list);
						//send serv leave to channelNodes' adj list
						struct node* temp = channelNode->adj_list->next;
						while (temp != NULL){
							strcpy(s_leave->txt_channel, channelNode->data);
							printf("sending serv leave\n");
							int flag = sendto(sockfd, s_leave, sizeof(s_leave), 0, (struct sockaddr*)temp->serv_addr, sizeof(struct sockaddr_in));
							if (flag == -1){
								printf("FAILED TO SEND SERV_LEAVE in req_leave\n");
							}
							temp = temp->next;
						}

						// remove channel
						remove_channel(channelNode->data, dll_channels);
					}
					channelNode = channelNode->next;
				}
				break;
			}
			case REQ_JOIN: { //join
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
					//create channel
					tempNode = append(r_join->req_channel, dll_channels, NULL);

					s_join = (struct serv_join*) malloc(sizeof(struct serv_join));
					s_join->serv_type = SERV_JOIN;
					//send serv_join to adjacent servers
					strcpy(s_join->txt_channel, r_join->req_channel);
					struct node* current = serv_self->next;
					while (current != NULL){
						//send join to each server in server adj list
						int flag = sendto(sockfd, s_join, sizeof(struct serv_join), 0, (struct sockaddr*)current->serv_addr, sizeof(struct sockaddr_in));
						if (flag == -1){
							printf("SEND FAILED\n");
						}
						//add each server in server adj list to new channels' adj list
						append(current->data, tempNode->adj_list, current->serv_addr);

						printf("%s %s send S2S Join %s\n",serv_self->data, current->data, s_join->txt_channel);
						printf("adj_serv_addr port: %d, host: %d\n", current->serv_addr->sin_port, current->serv_addr->sin_addr.s_addr);
						current = current->next;
					}
					free(s_join);
				}

				//finally channel is created, append user to it
				append(tempBuff, tempNode->inner, &serv_addr);

				printf("server: %s joins channel %s\n", tempBuff, tempNode->data);
				break;
			}
			case REQ_LEAVE: { //leave
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

				//if channel doesn't exist
				if(channelNode == NULL){
					//No channel by the name
					strcpy(t_error.txt_error, "No channel by the name ");
					strcat(t_error.txt_error, r_leave->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//remove user from channel
				if(remove_user(channelNode->inner, currentUserNode->serv_addr) == 0){
					strcpy(t_error.txt_error, "Not a member of channel ");
					strcat(t_error.txt_error, r_leave->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//remove empty channels
				channelNode = dll_channels->next;
				s_leave = (struct serv_leave*) malloc(sizeof(struct serv_leave));
				s_leave->serv_type = SERV_LEAVE;

				printf("going into while\n");
				while(channelNode != NULL){
					//if channel has no users AND channel has at most 1 serv in adj list
					if((channelNode->inner->next == NULL) && (channelNode->adj_list->numNodesInList <= 1)){
						printf("need to send leave message\n");
						displayData(channelNode->adj_list);
						//send serv leave to channelNodes' adj list
						struct node* temp = channelNode->adj_list->next;
						while (temp != NULL){
							strcpy(s_leave->txt_channel, channelNode->data);
							printf("sending serv leave\n");
							int flag = sendto(sockfd, s_leave, sizeof(s_leave), 0, (struct sockaddr*)temp->serv_addr, sizeof(struct sockaddr_in));
							if (flag == -1){
								printf("FAILED TO SEND SERV_LEAVE in req_leave\n");
							}
							temp = temp->next;
						}

						//remove channel
						remove_channel(channelNode->data, dll_channels);

					}
					channelNode = channelNode->next;
				}
				free(s_leave);
				printf("server: %s leaves channel %s\n", currentUserNode->data, r_leave->req_channel);

				break;
			}
			case REQ_SAY: { //say
				r_say = (struct request_say*) gen_request_struct;

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//take action - send message to ever user in channel
				struct node* channelNode;
				struct node* userNode;

				channelNode = find_channel(r_say->req_channel, dll_channels);
				if(channelNode == NULL){
					printf("server: %s trying to send to users in non-existing channel %s\n", currentUserNode->data, r_say->req_channel);
					strcpy(t_error.txt_error, "No channel by the name ");
					strcat(t_error.txt_error, r_say->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//for each user in the channel, send the say msg to that user
				userNode = channelNode->inner->next; //userNode is first user in channel
				/*
				if(userNode == NULL){ //sending say to empty channel, this should never happen as the channel should be removed when emptied
					printf("server: %s trying to send to users in an empty channel %s. Should not be possible.\n", currentUserNode->data, r_say->req_channel);
					strcpy(t_error.txt_error, "No users in channel ");
					strcat(t_error.txt_error, r_say->req_channel);
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				}
				*/

				printf("server: %s sends say message in %s\n",currentUserNode->data ,r_say->req_channel);
				while(userNode != NULL){ //send say to each user in channel
					t_say.txt_type = TXT_SAY;
					strcpy(t_say.txt_channel, r_say->req_channel);
					strcpy(t_say.txt_username, currentUserNode->data);
					strcpy(t_say.txt_text, r_say->req_text);
					sendto(sockfd, &t_say, sizeof(struct text_say), 0, (struct sockaddr*)userNode->serv_addr,  sizeof(serv_addr));
					userNode = userNode->next;
				}

				//send serv_say to all servers in channels adj list
				s_say = (struct serv_say*) malloc(sizeof(struct serv_say));
				unsigned char number[16];

				uuid_generate(number);

				s_say->UID = (long long) number;
				s_say->serv_type = SERV_SAY;
				strcpy(s_say->txt_channel, r_say->req_channel);
				strcpy(s_say->txt_text, r_say->req_text);
				strcpy(s_say->txt_username, currentUserNode->data);

				struct node* currentServer = channelNode->adj_list->next;
				while(currentServer != NULL){
					//send serv say
					int flag = sendto(sockfd, s_say, sizeof(struct serv_say), 0, (struct sockaddr*)currentServer->serv_addr,  sizeof(serv_addr));
					if (flag == -1){
						printf("FAILED TO SEND SERV_SAY\n");
					}

					currentServer = currentServer->next;
				}


				break;
			}
			case REQ_LIST: { //list
				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//take action - send a list all channels
				struct node* currentNode = dll_channels->next;
				int structSize = sizeof(struct text_list) + (dll_channels->numNodesInList * sizeof(struct channel_info));
				struct text_list* t_list = (struct text_list*)malloc(structSize);
				if (t_list == NULL){
					fprintf(stderr,"failed malloc\n");
					continue;
				}

				t_list->txt_type = TXT_LIST;
				t_list->txt_nchannels = dll_channels->numNodesInList;

				int i = 0;
				while(currentNode != NULL){
					strcpy(t_list->txt_channels[i].ch_channel, currentNode->data);
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

				//take action - send channel name and list of every user in that channel
				int numInChannel = channelNode->inner->numNodesInList;
				int structSize = sizeof(struct text_who) + (numInChannel * sizeof(struct user_info));
				struct text_who* t_who = (struct text_who*)malloc(structSize);
				if (t_who == NULL){
					fprintf(stderr,"failed malloc\n");
					continue;
				}

				//set fields
				t_who->txt_type = TXT_WHO;
				strcpy(t_who->txt_channel, r_who->req_channel);
				t_who->txt_nusernames =numInChannel;

				//populate channel array
				int i = 0;
				userNode = channelNode->inner->next;
				while(userNode != NULL){
					strcpy(t_who->txt_users[i].us_username, userNode->data);
					userNode = userNode->next;
					i++;
				}

				//send to whoever just asked for who
				sendto(sockfd, t_who, structSize, 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				free(t_who);
				break;
			}
			case SERV_JOIN:{
				printf("in serv join\n");
				//cast to specific type
				s_join = (struct serv_join*) gen_request_struct;

				//check if member of channel
				struct node* channel;
				channel = find_channel(s_join->txt_channel, dll_channels);

				//if not member, join channel then send serv_join to all adj servers
				if(channel == NULL){
					channel = append(s_join->txt_channel, dll_channels, NULL);
					printf("added new channel\n");

					//set channel adj list to be all adj channels and send join to all channels (but the one that sent us the join?)
					struct node* current;
					current = dll_adjacency->next->next;
					printf("building channel adj list\n");
					while (current != NULL){
						//add to channel adj list
						append(current->data, channel->adj_list, current->serv_addr);
						printf("building. appended\n");
						//send join to each channel in server adj list
						int flag = sendto(sockfd, s_join, sizeof(s_join), 0, (struct sockaddr*)current->serv_addr, sizeof(struct sockaddr_in));
						if (flag == -1){
							printf("FAILED TO SEND SERV_JOIN\n");
						}
						printf("%s %s send S2S Join %s\n",serv_self->data, current->data, s_join->txt_channel);
						current = current->next;
					}

				}
				printf("completed serv join\n");
				break;
			}
			case SERV_LEAVE:{
				printf("in serv leave\n");
				//cast to specific type
				s_leave = (struct serv_leave*) gen_request_struct;

				//get channel name
				struct node* channel;
				channel = find_channel(s_leave->txt_channel, dll_channels);

				//if we are subscribed to channel
				if(channel != NULL){
					//remove server from the channel's adj list (if present)
					remove_user(channel->adj_list, &serv_addr);

					int adjLen = channel->adj_list->numNodesInList;
					int clientLen = channel->inner->numNodesInList;
					//if len of channel adj list <= 1 AND channel has no clients attached
					if((adjLen <= 1) && (clientLen == 0)){
						//send leave to servers in channels adj list
						struct node* current;
						current = channel->adj_list->next;
						while (current != NULL){
							int flag = sendto(sockfd, s_leave, sizeof(s_leave), 0, (struct sockaddr*)current->serv_addr, sizeof(struct sockaddr_in));
							if (flag == -1){
								printf("FAILED TO SEND SERV_LEAVE\n");
							}

							current = current->next;
						}

						//remove channel from channel list
						remove_channel(channel->data, dll_channels);
					}
				}
				break;
			}
			case SERV_SAY:{
				//cast to specific type
				s_say = (struct serv_say*) gen_request_struct;

				//if new say msg
				int i;
				int oldMessage = 0;
				for (i = 0; i < ID_MAX; i++){
					if (SAY_IDS[i] == s_say->UID){
						oldMessage = 1;
					}
				}
				if (!oldMessage){
					//send say msg to all users in the channel
					struct node* channel = find_channel(s_say->txt_channel, dll_channels);
					struct node* user = channel->next;
					while(user != NULL){
						t_say.txt_type = TXT_SAY;
						strcpy(t_say.txt_channel, s_say->txt_channel);
						strcpy(t_say.txt_username, s_say->txt_username);
						strcpy(t_say.txt_text, s_say->txt_text);
						int flag = sendto(sockfd, &t_say, sizeof(struct text_say), 0, (struct sockaddr*)user->serv_addr,  sizeof(serv_addr));
						if (flag == -1){
							printf("FAILED TO SEND TEXT_SAY\n");
						}
						user = user->next;
					}

					//send serv_say to all adj servs in the channel BUT the one that sent us a serv_say
					struct node* currentServer = channel->adj_list->next;
					while(currentServer != NULL){
						//if currentServer not the serv who sent to us
						if(!((currentServer->serv_addr->sin_port == serv_addr.sin_port) && (currentServer->serv_addr->sin_addr.s_addr == serv_addr.sin_addr.s_addr))){
							//send serv say
							int flag = sendto(sockfd, s_say, sizeof(struct serv_say), 0, (struct sockaddr*)currentServer->serv_addr,  sizeof(serv_addr));
							if (flag == -1){
								printf("FAILED TO SEND SERV_SAY\n");
							}
						}
						currentServer = currentServer->next;
					}
				}
				else{
					//send leave
					s_leave = (struct serv_leave*) malloc(sizeof(struct serv_leave));
					s_leave->serv_type = SERV_LEAVE;
					strcpy(s_leave->txt_channel, s_say->txt_channel);

					int flag = sendto(sockfd, s_leave, sizeof(struct serv_leave), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					if (flag == -1){
						printf("FAILED TO SEND SERV_SAY\n");
					}

					free(s_leave);
				}
				break;
			}
			default:{
				strcpy(t_error.txt_error, "Server couldn't match message type.");
				sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				fprintf(stderr, "ERROR - server: no matching req type\n");
			}
		}
	}

	printf("server exiting. This should not normally happen.\n");
	removeAll(dll_channels);
	removeAll(dll_users);
	return 0;
}
