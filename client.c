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
#include <sys/select.h>

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

//this link always point to first Link
struct node *head = NULL;

//this link always point to tail Link
struct node *tail = NULL;

struct node *current = NULL;

struct node {
   char data[32];

   struct node *next;
   struct node *prev;
};

//is list empty
int isEmpty() {
	if (head == NULL)
			return 1;
	else
		return 0;
}

//display the list in from head to tail
void displayForward() {

   //start from the beginning
   struct node *ptr = head;

   //navigate till the end of the list
   printf("[ ");

   while(ptr != NULL) {
      printf("data:%s,  ", ptr->data);
      ptr = ptr->next;
   }

   printf(" ]\n");
}

//insert link at the tail location
void insertTail(char* data) {

   //create a link
   struct node *link = (struct node*) malloc(sizeof(struct node));
   strcpy(link->data, data);

   if(isEmpty()) {
       //make it the head and tail
	   head = link;
       tail = link;
   }
   else {
	   //make link a new tail link
	   tail->next = link;

	   //mark old tail node as prev of new link
	   link->prev = tail;
   }

   //point tail to new tail node
   tail = link;
}

//remove link at the tail location
void removeTail() {
	if(tail == NULL){
		return;
	}
   //save reference to tail link
   struct node *tempLink = tail;

   //if only one link
   if(head->next == NULL) {
      head = NULL;
   } else {
      tail->prev->next = NULL;
   }

   tail = tail->prev;

   free(tempLink);
}

int contains(char* data){
	//start from the first link
	struct node* current = head;

	//if list is empty
	if(head == NULL) {
	  return 0;
	}

	//navigate through list
	while(strcmp(current->data, data)) {
		//if it is tail node

		if(current->next == NULL) {
			return 0;
		}
		else {
			//move to next link
			current = current->next;
		}
	}
	return 1;
}

//remove a link with given data
void* remove(char* data) {

   //start from the first link
   struct node* current = head;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
   while(strcmp(current->data, data)) {
      //if it is tail node

      if(current->next == NULL) {
         return NULL;
      } else {
         //move to next link
         current = current->next;
      }
   }

   //found a match, update the link
   if(current == head) {
      //change first to point to next link
      head = head->next;
   } else {
      //bypass the current link
      current->prev->next = current->next;
   }

   if(current == tail) {
      //change tail to point to prev link
      tail = current->prev;
   } else {
      current->next->prev = current->prev;
   }
   free(current);
   return NULL;
}

void destroyDDL(){
	while(!isEmpty()){
		removeTail();
	}
}

void handleRead(int readResult){
	if (readResult < 0){
		printf("ERROR reading from socket\n");
		exit(0);
	}
}


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
	int sockfd, server_port;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char username[25]; 
	char DEF_CHAN[32] = "Common";

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

	//Statically allocate request structs and set codes
	struct request_login req_login;
	req_login.req_type = REQ_LOGIN;
	struct request_logout  req_logout;
	req_logout.req_type = REQ_LOGOUT;
	struct request_join req_join;
	req_join.req_type = REQ_JOIN;
	struct request_leave req_leave;
	req_leave.req_type = REQ_LEAVE;
	struct request_say req_say;
	req_say.req_type = REQ_SAY;
	struct request_list req_list;
	req_list.req_type = REQ_LIST;
	struct request_who req_who;
	req_who.req_type = REQ_WHO;

	//init to chat server - login, join common
	//login
	req_login.req_type = REQ_LOGIN;
	strcpy(req_login.req_username, username);
	sendto(sockfd, &req_login, sizeof(struct request_login), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

	//join common
	req_join.req_type = REQ_JOIN;
	strcpy(req_join.req_channel, DEF_CHAN);
	sendto(sockfd, &req_join, sizeof(struct request_join), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

	//while logged in, handle user input, handle server messages
	char current_char;
	int inChannel = 1;
	char cur_channel[32];
	int UDP_MAX_PAC_SIZE = 65507;
	char buf[UDP_MAX_PAC_SIZE];
	int logged_in = 1;
	int input_buff_ctr = 0;
	int newline_flag = 0;
	int msg_flag = 0;
	char input_buff[128];
	input_buff[0] = '\0';
	struct text* gen_received_struct;
	struct text_say* t_say = NULL;
	struct text_list* t_list = NULL;
	struct text_who* t_who = NULL;
	struct text_error* t_error = NULL;

    fd_set s_rd;

	strcpy(cur_channel, DEF_CHAN);
	printf("> ");
	fflush(stdout);
	while(logged_in){

		FD_ZERO(&s_rd);
		FD_SET(sockfd, &s_rd);
		FD_SET(fileno(stdin), &s_rd);

		//see if any fds are ready
		select(sockfd+1, &s_rd, NULL, NULL, NULL);
		// there is a message from the server
		if (FD_ISSET(0, &s_rd)){
			//read and print the next char in stdin
			current_char = (char)fgetc(stdin);
			printf("%c", current_char);
			fflush(stdout);

			if (current_char == '\n'){
						newline_flag = 1;
						if (!strcmp(input_buff, "/exit")){
							sendto(sockfd, &req_logout, sizeof(struct request_logout), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
							logged_in = 0;
							newline_flag = 0; //don't print prompt
						}
						else if (!strncmp(input_buff, "/join ", 6 * sizeof(char))){
							inChannel = 1;
							strcpy(cur_channel, input_buff+6);

							//parse channel name from input_buff
							strcpy(req_join.req_channel, cur_channel);
							sendto(sockfd, &req_join, sizeof(struct request_join), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

							//add to channel list
							remove(cur_channel); //remove dup
							insertTail(cur_channel);

							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr+1] = '\0';
						}
						else if (!strncmp(input_buff, "/leave ", 7 * sizeof(char))){
							//parse channel name from input_buff
							strcpy(req_leave.req_channel, input_buff+7);
							sendto(sockfd, &req_leave, sizeof(struct request_leave), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));

							remove(input_buff+7);

							//if leaving current channel
							if(!strcmp(input_buff+7,cur_channel)){
								inChannel = 0;
							}


							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr+1] = '\0';
						}
						else if (!strncmp(input_buff, "/list", 5 * sizeof(char))){
							sendto(sockfd, &req_list, sizeof(struct request_list), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr+1] = '\0';
						}
						else if (!strncmp(input_buff, "/who ", 5 * sizeof(char))){
							//parse channel name from input_buff
							strcpy(req_who.req_channel, input_buff+5);
							sendto(sockfd, &req_who, sizeof(struct request_who), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr+1] = '\0';
						}
						else if (!strncmp(input_buff, "/switch ", 8 * sizeof(char))){
							//if client is a member of the specified channel
							if (contains(input_buff+8)){
								//set active channel to be that channel
								strcpy(cur_channel, input_buff+8);
							}

							else{
								//display error and don't move channel
								printf("You have not subscribed to channel %s\n", input_buff+8);
							}

							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr] = '\0';
						}
						else if(!strncmp(input_buff, "/", sizeof(char))){
							printf("%s", "*Unknown command\n");

							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr+1] = '\0';
						}

						else{ //not a command, must be a message to be sent
							if(inChannel){
								req_say.req_type = REQ_SAY;

								//make message from input_buffer
								strcpy(req_say.req_channel, cur_channel);
								strcpy(req_say.req_text, input_buff);

								//send message
								sendto(sockfd, &req_say, sizeof(struct request_say), 0, (struct sockaddr*)&serv_addr,  sizeof(serv_addr));
							}
							//clear input_buffer
							input_buff_ctr = 0;
							input_buff[input_buff_ctr] = '\0';
						}
						fflush(stdout);
					}
					else{
						//put the new char in the buff
						input_buff[input_buff_ctr++] = current_char;
						input_buff[input_buff_ctr] = '\0';
					}

		}

		if (FD_ISSET(sockfd, &s_rd)){
			msg_flag = 0;

			//print backspaces to clear
			printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

			//read what whole UDP packet
			bzero(buf, UDP_MAX_PAC_SIZE);
			handleRead(read(sockfd, buf, UDP_MAX_PAC_SIZE));

			//cast as generic type and get code
			gen_received_struct = (struct text*)buf;

			//if say
			if(gen_received_struct->txt_type == 0){
				t_say = ((struct text_say*) gen_received_struct);
				printf("[%s][%s]: %s\n", t_say->txt_channel, t_say->txt_username, t_say->txt_text);
			}

			//if list
			else if(gen_received_struct->txt_type == 1){
				t_list = ((struct text_list*) gen_received_struct);
				printf("Existing channels:\n");
				int i;
				for(i = 0; i < t_list->txt_nchannels; i++){
					printf(" %s\n", (char*)t_list->txt_channels + i);
				}
			}

			//if who
			else if(gen_received_struct->txt_type == 2){
				t_who = ((struct text_who*) gen_received_struct);
				printf("Users on channel Common:\n");
				int j;
				for(j=0; j<t_who->txt_nusernames; j++){
					printf(" %s\n", (char*)t_who->txt_users + j);

				}
			}

			//if error
			else if(gen_received_struct->txt_type == 3){
				t_error = ((struct text_error*) gen_received_struct);
				printf("%s\n", t_error->txt_error);
			}

			//redisplay input buff
			printf("> %s", input_buff);

			fflush(stdout);
		}

		//f newline or message, prompt
		if (newline_flag || msg_flag){
			printf("> ");
			newline_flag = 0;
			msg_flag = 0;
		}
		fflush(stdout);
	}



	destroyDDL();
	close(sockfd);
	return 0;
}


