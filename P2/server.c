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
#include <signal.h>
#include <fcntl.h> //Open file
#include "listOfLists.h"
#include "duckchat.h"
#include "raw.h"

#define ADDRESS_MAX 64
#define ID_MAX 1000
#define UNUSED __attribute__((unused))
#define ADR_SIZE 22

struct node* dll_channels;
struct node* dll_users;
struct node* dll_adjacency;

struct node* serv_self;

long long SAY_IDS[ID_MAX];
int CUR_ID_POS = 0;
int sockfd;
int isMin = 1;

void handleRead(int readResult){
	if (readResult < 0){
		printf("ERROR reading from socket\n");
		exit(0);
	}
}

static void onalrm(UNUSED int sig) {
	//on alarm called every TIME_PERIOD
	signal(SIGINT, SIG_IGN);

	//printf("received alarm \n");
	//for each channel send join msgs to the servers adj list
	struct node* channel = dll_channels->next;
	struct node* adj_server;
	int flag;

	struct serv_join* s_join = (struct serv_join*)malloc(sizeof(struct serv_join));
	s_join->serv_type = SERV_JOIN;

	//while(channel != NULL){
	while(0){ //TODO SEGFAULTS
		strcpy(s_join->txt_channel, channel->data);

		adj_server = channel->adj_list->next;
		while (adj_server != NULL){
			flag = sendto(sockfd, s_join, sizeof(struct serv_join), 0, (struct sockaddr*)adj_server->serv_addr, sizeof(struct sockaddr_in));
			if (flag == -1){
				printf("FAILED TO SEND SERV_JOIN1 in alarm\n");
			}
			//PRINT
			//sender
			char send_buff[ADR_SIZE];
			inet_ntop(AF_INET, &adj_server->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
			int send_port = ntohs(adj_server->serv_addr->sin_port);

			//self
			char self_buff[ADR_SIZE];
			inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
			int self_port = ntohs(serv_self->serv_addr->sin_port);

			printf("%s:%d %s:%d send S2S Join - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_join->txt_channel);



			//if this is a 2 minute timer, remove dead servers from channel adj list
			if(!isMin){
					//if server keep alive 1, set 0
					if(adj_server->isAlive == 1){
						adj_server->isAlive = 0;
					}
					//if server keep alive 0, remove server form channels adj list
					else{
						remove_user(channel->adj_list, adj_server->serv_addr);
					}
			}
			adj_server = adj_server->next;
		}

		channel = channel->next;
	}
	isMin = !isMin;
	signal(SIGINT, SIG_DFL);
}

void setSignalHandlers(){
	//set sigusr1 handlers
    if (signal(SIGALRM, onalrm) == SIG_ERR) {
    	printf("Can't establish SIGUSR1 handler\n");
    	exit(1);
    }
}


int main(int argc, char *argv[]){
	raw_mode(); //set raw
	atexit(cooked_mode); //return to cooked on normal exit
	//printf("size of int %lu\n", sizeof(int));
	//printf("size of long long %lu\n", sizeof(long long));
	//printf("size of unsigned int %lu\n", sizeof(unsigned int));

	if (argc < 3){
		printf("Usage: ./server domain_name port_num ...\n");
		return 1;
	}
	if ((argc % 2) != 1){
		printf("Error - Pairs of hosts and ports incomplete");
		return 1;
	}

	setSignalHandlers();

	//zero out UUID array
	int j;
	for (j = 0; j < ID_MAX; j++){
		SAY_IDS[j] = 0;
	}

	//seed srand
	char buf[sizeof(unsigned int)];
	//get path to dev/urandom
	//read 4 bytes
	int fd = open("/dev/urandom", 0);
	read(fd, buf, sizeof(unsigned int));
	//
	uint32_t randbuf;
	memcpy(&randbuf, buf, sizeof(uint32_t));
	srand((unsigned int)randbuf);

	struct itimerval it_val;
	it_val.it_value.tv_sec = 20;
	it_val.it_value.tv_usec = 0;
	it_val.it_interval = it_val.it_value;
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		printf("error calling setitimer()");
		exit(1);
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
		strcat(serverName, argv[i]);
		strcat(serverName, argv[i+1]);

		//create server sockaddr_in
		adj_server_port = atoi(argv[i+1]);
		//printf("byte order port %d\n", htons(adj_server_port));
		if ((adj_server_address = gethostbyname(argv[i])) == NULL) {
			fprintf(stderr, "ERROR - server: no such host\n");
			return 1;
		}

		//create serv_addr
		bzero((char *)&adj_serv_addr, sizeof(adj_serv_addr));
		adj_serv_addr.sin_family = AF_INET;
		bcopy((char *)adj_server_address->h_addr, (char *)&adj_serv_addr.sin_addr.s_addr, adj_server_address->h_length);
		adj_serv_addr.sin_port = htons(adj_server_port);

		char buff[ADR_SIZE];
		inet_ntop(AF_INET, &adj_serv_addr.sin_addr.s_addr, buff, ADR_SIZE);
		//printf("adj_serv_addr port: %d, host: %s\n", ntohs(adj_serv_addr.sin_port), buff);

		append(serverName, dll_adjacency, &adj_serv_addr);
		serverName[0] ='\0';
	}
	serv_self = dll_adjacency->next;

	//printf("adjacency list created\n");

	//create socket
	int server_port, loggedIn;
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
		//printf("\trecieved message!\n");

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

				dll_users->numNodesInList++;

				//PRINT
				//sender
				char send_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
				int send_port = ntohs(serv_addr.sin_port);

				//self
				char self_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
				int self_port = ntohs(serv_self->serv_addr->sin_port);

				printf("%s:%d %s:%d recv Request Login User: %s\n", self_buff, self_port, send_buff, send_port, r_login->req_username);

				break;
			}
			case REQ_LOGOUT: { //logout
				if(!loggedIn){
					break;
				}
				//PRINT
				//sender
				char send_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
				int send_port = ntohs(serv_addr.sin_port);

				//self
				char self_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
				int self_port = ntohs(serv_self->serv_addr->sin_port);

				printf("%s:%d %s:%d recv Request Logout User: %s\n", self_buff, self_port, send_buff, send_port, currentUserNode->data);

				//remove user from user list
				remove_user(dll_users, &serv_addr);

				//remove user from all channels on server
				struct node* channelNode;
				channelNode = dll_channels->next;
				while(channelNode != NULL){
					//remove user if contained in channel
					remove_user(channelNode->inner, &serv_addr);

					channelNode = channelNode->next;
				}
				break;
			}
			case REQ_JOIN: { //join
				r_join = (struct request_join*) gen_request_struct;
				struct node* channel;
				char tempBuff[USERNAME_MAX];

				//PRINT
				//sender
				char send_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
				int send_port = ntohs(serv_addr.sin_port);

				//self
				char self_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
				int self_port = ntohs(serv_self->serv_addr->sin_port);

				printf("%s:%d %s:%d recv Request Join - Channel: %s\n", self_buff, self_port, send_buff, send_port, r_join->req_channel);

				//if user not logged in
				if(!loggedIn){
					strcpy(t_error.txt_error, "Not logged in");
					sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
					break;
				}

				//get user name of join request
				strcpy(tempBuff, currentUserNode->data);

				//if channel not created
				if((channel = find_channel(r_join->req_channel, dll_channels)) == NULL){
					//create channel
					channel = append(r_join->req_channel, dll_channels, NULL);

					s_join = (struct serv_join*) malloc(sizeof(struct serv_join));
					s_join->serv_type = SERV_JOIN;
					//send serv_join to adjacent servers
					strcpy(s_join->txt_channel, r_join->req_channel);
					struct node* serv = dll_adjacency->next->next;
					while (serv != NULL){
						//send join to each server in server adj list
						int flag = sendto(sockfd, s_join, sizeof(struct serv_join), 0, (struct sockaddr*)serv->serv_addr, sizeof(struct sockaddr_in));
						if (flag == -1){
							printf("FAILED TO SEND SERV_JOIN2\n");
						}

						//PRINT
						//sender
						char send_buff[ADR_SIZE];
						inet_ntop(AF_INET, &serv->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
						int send_port = ntohs(serv->serv_addr->sin_port);

						//self
						char self_buff[ADR_SIZE];
						inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
						int self_port = ntohs(serv_self->serv_addr->sin_port);

						printf("%s:%d %s:%d send S2S Join - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_join->txt_channel);

						//add each server in server adj list to new channels' adj list
						append(serv->data, channel->adj_list, serv->serv_addr);

						serv = serv->next;
					}
					free(s_join);
				}

				//finally channel is created, append user to it
				append(tempBuff, channel->inner, &serv_addr);
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

				//PRINT
				//sender
				char send_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
				int port = ntohs(serv_addr.sin_port);

				//self
				char self_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
				int self_port = ntohs(serv_self->serv_addr->sin_port);

				printf("%s:%d %s:%d recv Request Leave Channel: %s\n", send_buff, port, self_buff, self_port, r_leave->req_channel);

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

				//PRINT
				//sender
				char send_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
				int send_port = ntohs(serv_addr.sin_port);

				//self
				char self_buff[ADR_SIZE];
				inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
				int self_port = ntohs(serv_self->serv_addr->sin_port);

				printf("%s:%d %s:%d recv Request Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, currentUserNode->data, r_say->req_channel, r_say->req_text);

				while(userNode != NULL){ //send say to each user in channel
					t_say.txt_type = TXT_SAY;
					strcpy(t_say.txt_channel, r_say->req_channel);
					strcpy(t_say.txt_username, currentUserNode->data);
					strcpy(t_say.txt_text, r_say->req_text);
					sendto(sockfd, &t_say, sizeof(struct text_say), 0, (struct sockaddr*)userNode->serv_addr,  sizeof(serv_addr));

					//PRINT
					//sender
					char send_buff[ADR_SIZE];
					inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
					int send_port = ntohs(serv_addr.sin_port);

					//self
					char self_buff[ADR_SIZE];
					inet_ntop(AF_INET, &userNode->serv_addr->sin_addr, self_buff, ADR_SIZE);
					int self_port = ntohs(userNode->serv_addr->sin_port);

					printf("%s:%d %s:%d send Request Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, userNode->data, r_say->req_channel, r_say->req_text);

					userNode = userNode->next;
				}

				//send serv_say to all servers in channels adj list
				s_say = (struct serv_say*) malloc(sizeof(struct serv_say));

				s_say->UID = (long long) rand();
				s_say->serv_type = SERV_SAY;
				strcpy(s_say->txt_channel, r_say->req_channel);
				strcpy(s_say->txt_text, r_say->req_text);
				strcpy(s_say->txt_username, currentUserNode->data);

				struct node* currentServer = channelNode->adj_list->next;
				while(currentServer != NULL){
					//send serv say
					int flag = sendto(sockfd, s_say, sizeof(struct serv_say), 0, (struct sockaddr*)currentServer->serv_addr,  sizeof(serv_addr));
					if (flag == -1){
						printf("FAILED TO SEND SERV_SAY1\n");
						//TODO REMOVE

						displayData(channelNode->adj_list->next);
						//PRINT
						//sender
						char send_buff[ADR_SIZE];
						inet_ntop(AF_INET, &currentServer->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
						int send_port = ntohs(currentServer->serv_addr->sin_port);

						//self
						char self_buff[ADR_SIZE];
						inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
						int self_port = ntohs(serv_self->serv_addr->sin_port);

						printf("%s:%d %s:%d send S2S Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, s_say->txt_username, s_say->txt_channel, s_say->txt_text);
						break;
					}
					printf("send success\n");
					//PRINT
					//sender
					char send_buff[ADR_SIZE];
					inet_ntop(AF_INET, &currentServer->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
					int send_port = ntohs(currentServer->serv_addr->sin_port);

					//self
					char self_buff[ADR_SIZE];
					inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
					int self_port = ntohs(serv_self->serv_addr->sin_port);

					printf("%s:%d %s:%d send S2S Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, s_say->txt_username, s_say->txt_channel, s_say->txt_text);

					currentServer = currentServer->next;
				}
				free(s_say);

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
				//cast to specific type
				s_join = (struct serv_join*) gen_request_struct;

				//check if member of channel
				struct node* channel;
				channel = find_channel(s_join->txt_channel, dll_channels);

				//if not member, join channel then send serv_join to all adj servers
				if(channel == NULL){
					channel = append(s_join->txt_channel, dll_channels, NULL);

					//set channel adj list to be all adj servers and send serv_join to all channels (but the one that sent us the join)
					struct node* currentServer;
					currentServer = dll_adjacency->next->next;
					while (currentServer != NULL){
						//add surrentServer to channel adj list
						append(currentServer->data, channel->adj_list, currentServer->serv_addr);

						//send join to each channel in server adj list, but the one that sent us join
						if(!((currentServer->serv_addr->sin_port == serv_addr.sin_port) && (currentServer->serv_addr->sin_addr.s_addr == serv_addr.sin_addr.s_addr))){
							int flag = sendto(sockfd, s_join, sizeof(struct serv_join), 0, (struct sockaddr*)currentServer->serv_addr, sizeof(struct sockaddr_in));
							if (flag == -1){
								printf("FAILED TO SEND SERV_JOIN3\n");
							}
							//PRINT
							//sender
							char send_buff[ADR_SIZE];
							inet_ntop(AF_INET, &currentServer->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
							int send_port = ntohs(currentServer->serv_addr->sin_port);

							//self
							char self_buff[ADR_SIZE];
							inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
							int self_port = ntohs(serv_self->serv_addr->sin_port);

							printf("%s:%d %s:%d send S2S Join - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_join->txt_channel);
						}
						currentServer = currentServer->next;
					}
				}
				//else we are a member, set the isAlive of the server in the channel to 1.
				else{
					struct node* server = find_user(channel->adj_list, &serv_addr);
					server->isAlive = 1;
				}
				break;
			}
			case SERV_LEAVE:{
				//cast to specific type
				s_leave = (struct serv_leave*) gen_request_struct;

				//get channel name
				struct node* channel;
				channel = find_channel(s_leave->txt_channel, dll_channels);

				//if we are subscribed to channel
				if(channel != NULL){
					//PRINT
					//sender
					char send_buff[ADR_SIZE];
					inet_ntop(AF_INET, &adj_serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
					int send_port = ntohs(adj_serv_addr.sin_port);

					//self
					char self_buff[ADR_SIZE];
					inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
					int self_port = ntohs(serv_self->serv_addr->sin_port);

					printf("%s:%d %s:%d recv S2S Leave - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_leave->txt_channel);

					//remove server from the channel's adj list (if present)
					remove_user(channel->adj_list, &serv_addr);
				}
				break;
			}
			case SERV_SAY:{
				printf("received s2s say\n");
				//cast to specific type
				s_say = (struct serv_say*) gen_request_struct;
				printf("UUID IS %lld\n", s_say->UID);

				//check if duplicate UUID
				int i;
				int oldMessage = 0;
				for (i = 0; i < ID_MAX; i++){
					if (SAY_IDS[i] == s_say->UID){
						oldMessage = 1;
					}
				}
				struct node* channel = find_channel(s_say->txt_channel, dll_channels);
				//if say to channel we aren't subscribed to, do nothing
				if(channel == NULL){
					break;
				}

				//if the server has users in the channel
				if(channel->inner->next != NULL){
					printf("server has users in channel\n");
					if (!oldMessage){
						printf("not old msg\n");
						//add UUID to list
						SAY_IDS[CUR_ID_POS] = s_say->UID;
						CUR_ID_POS = (CUR_ID_POS + 1) % ID_MAX;

						//send say msg to all users in the channel
						struct node* user = channel->inner->next;
						while(user != NULL){
							t_say.txt_type = TXT_SAY;
							strcpy(t_say.txt_channel, s_say->txt_channel);
							strcpy(t_say.txt_username, s_say->txt_username);
							strcpy(t_say.txt_text, s_say->txt_text);
							int flag = sendto(sockfd, &t_say, sizeof(struct text_say), 0, (struct sockaddr*)user->serv_addr,  sizeof(serv_addr));
							if (flag == -1){
								printf("FAILED TO SEND TEXT_SAY\n");
							}
							//PRINT
							//sender
							char send_buff[ADR_SIZE];
							inet_ntop(AF_INET, &user->serv_addr->sin_addr.s_addr, send_buff, ADR_SIZE);
							int send_port = ntohs(user->serv_addr->sin_port);

							//self
							char self_buff[ADR_SIZE];
							inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
							int self_port = ntohs(serv_self->serv_addr->sin_port);

							printf("%s:%d %s:%d send Test Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, s_say->txt_username, s_say->txt_channel, s_say->txt_text);

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
									printf("FAILED TO SEND SERV_SAY2\n");
								}
								//PRINT
								//sender
								char send_buff[ADR_SIZE];
								inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
								int send_port = ntohs(serv_addr.sin_port);

								//self
								char self_buff[ADR_SIZE];
								inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
								int self_port = ntohs(serv_self->serv_addr->sin_port);

								printf("%s:%d %s:%d send S2S Say - User: %s Channel: %s Msg: \"%s\"\n", self_buff, self_port, send_buff, send_port, s_say->txt_username, s_say->txt_channel, s_say->txt_text);

							}
							currentServer = currentServer->next;
						}
					}
					else{
						//duplicate say
						printf("old msg\n");
						//send leave to the one connection
						s_leave = (struct serv_leave*) malloc(sizeof(struct serv_leave));
						s_leave->serv_type = SERV_LEAVE;
						strcpy(s_leave->txt_channel, s_say->txt_channel);

						int flag = sendto(sockfd, s_leave, sizeof(struct serv_leave), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
						if (flag == -1){
							printf("FAILED TO SEND SERV_SAY3\n");
						}
						//PRINT
						//sender
						char send_buff[ADR_SIZE];
						inet_ntop(AF_INET, &adj_serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
						int send_port = ntohs(adj_serv_addr.sin_port);

						//self
						char self_buff[ADR_SIZE];
						inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
						int self_port = ntohs(serv_self->serv_addr->sin_port);

						printf("%s:%d %s:%d Send S2S Leave - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_leave->txt_channel);

						free(s_leave);
					}
				}
				else{
					printf("channel has no users \n");
					//channel has no users
					//if channel is a leaf:
					if(channel->adj_list->numNodesInList >= 1){
						//send leave to the one connection
						s_leave = (struct serv_leave*) malloc(sizeof(struct serv_leave));
						s_leave->serv_type = SERV_LEAVE;
						strcpy(s_leave->txt_channel, s_say->txt_channel);

						int flag = sendto(sockfd, s_leave, sizeof(struct serv_leave), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
						if (flag == -1){
							printf("FAILED TO SEND SERV_SAY4\n");
						}
						//PRINT
						//sender
						char send_buff[ADR_SIZE];
						inet_ntop(AF_INET, &adj_serv_addr.sin_addr.s_addr, send_buff, ADR_SIZE);
						int send_port = ntohs(adj_serv_addr.sin_port);

						//self
						char self_buff[ADR_SIZE];
						inet_ntop(AF_INET, &serv_self->serv_addr->sin_addr, self_buff, ADR_SIZE);
						int self_port = ntohs(serv_self->serv_addr->sin_port);

						printf("%s:%d %s:%d Send S2S Leave - Channel: %s\n", self_buff, self_port, send_buff, send_port, s_leave->txt_channel);

						//remove channel
						remove_channel(channel->data, dll_channels);

						free(s_leave);
					}


				}
				break;
			}
			default:{
				//strcpy(t_error.txt_error, "Server couldn't match message type.");
				//sendto(sockfd, &t_error, sizeof(struct text_error), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
				fprintf(stderr, "ERROR - server: no matching struct type\n");
			}
		}
	}

	printf("server exiting. This should not normally happen.\n");
	removeAll(dll_channels);
	removeAll(dll_users);
	return 0;
}
