/*
 * listOfLists.h
 *
 *  Created on: Nov 1, 2017
 *      Author: brian
 *      This implementation of a DLL is a modified form of the DLL found at:
 *      https://www.tutorialspoint.com/data_structures_algorithms/doubly_linked_list_program_in_c.htm
 *
 */
#include <stdlib.h>
#include "duckchat.h"
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>

#ifndef LISTOFLISTS_H_
#define LISTOFLISTS_H_



int input_buff_ctr = 0;
char input_buff[SAY_MAX];

struct node {
   char data[32];
   int numNodesInList;
   struct sockaddr_in* serv_addr;

   struct node* inner;
   struct node* next;
   struct node* prev;
};

struct node* initDLL(){
	struct node *init_node = (struct node*) malloc(sizeof(struct node));
	init_node->next = NULL;
	init_node->prev = NULL;
	init_node->numNodesInList = 0;
	init_node->data[0] = '\0';
	init_node->inner = NULL;
	init_node->serv_addr = NULL;
	return init_node;
}

//is list empty
int isEmpty(struct node* head) {
	if (head->next == NULL)
		return 1;
	else
		return 0;
}

//takes start node, displays data from start->next on
void displayData(struct node* start) {

   //start from the beginning
   struct node *current = start;

   //navigate till the end of the list
   printf("[ ");

   while(current != NULL) {
      printf("%s, ", current->data);
      current = current->next;
   }

   printf(" ]\n");
}

//takes special node 'head' of list and returns the node that contains data. NULL if no such node
struct node* find_channel(char* data, struct node* head){
	struct node* current = head->next;
	if (current == NULL){
		return NULL;
	}
	while(strcmp(current->data, data)){
		current = current->next;
		if (current == NULL){
			return NULL;
		}
	}
	return current;
}

//returns user node, null if user not in list
struct node* find_user(struct node* head, struct sockaddr_in* address){
	struct node* current = head->next;
	if (current == NULL){
		//printf("\tlist looks empty\n");
		return NULL;
	}

	//while port and address don't match
	while(!((current->serv_addr->sin_port == address->sin_port) && (current->serv_addr->sin_addr.s_addr == address->sin_addr.s_addr))){
		current = current->next;
		if (current == NULL){
			//printf("\tnot in list\n");
			return NULL;
		}
	}
	//printf("\tfound user\n");
	return current;
}

//takes special node 'head' of list and appends to the end. returns new node on success, NULL otherwise
struct node* append(char* data, struct node* head, struct sockaddr_in* address) {
//	printf("\tappending. this list looks like: ");
//	displayData(head);

	//create a link
	struct node *link = (struct node*) malloc(sizeof(struct node));
	//set fields to NULL
	link->numNodesInList = 0;
	link->data[0] = '\0';
	link->inner = NULL;
	link->next = NULL;
	link->prev = NULL;
	link->serv_addr = NULL;

	if(address != NULL){//trying to add user
		if(find_user(head, address) == NULL){//trying to add new user
			link->serv_addr = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
			memcpy(link->serv_addr, address, sizeof(struct sockaddr_in));
		}
		else{//adding old user
			free(link);
			//printf("\told user\n");
			return NULL;
		}
	}
	else { //trying to add channel
		if(find_channel(data, head) == NULL){ //trying to add new channel
			//printf("adding new channel\n");
			link->serv_addr = NULL;
		}
		else{ //adding old channel
			free(link);
			//printf("\tattempting to add old channel\n");
			return NULL;
		}
	}

	//initialize
	link->inner = initDLL();
	//link->serv_addr = NULL;
	strcpy(link->data, data);

	struct node* current = head;

	while(current->next != NULL){
		current = current->next;
	}
	current->next = link;
	link->prev = current;

	//	printf("\tfinish appending. this list looks like: ");
//	displayData(head);
	head->numNodesInList++;
	return link;
}

//takes a start node, removes and frees all next nodes
void* removeAll(struct node* start) {
	struct node* current = start;
	if(current == NULL){
		return NULL;
	}
	removeAll(current->next);
	removeAll(current->inner);
	if(current->serv_addr != NULL){
		free(current->serv_addr);
	}
	free(current);

	return NULL;
}

//takes special node 'head' of list and removes the node with given address if it exits. returns 1 on success
int remove_user(struct node* head, struct sockaddr_in* address) {
	struct node* current = head->next;
	//printf("current set\n");
	//if list is empty
	if(current == NULL) {
		//printf("current null\n");
		return 0;
	}
/*
	printf("current port: %d\n", current->serv_addr->sin_port);
	printf("current IP address: %d\n", current->serv_addr->sin_addr.s_addr);

	printf("looking for port: %d\n", address->sin_port);
	printf("looking for IP address: %d\n", address->sin_addr.s_addr);

	printf("compare port is %d\n", (current->serv_addr->sin_port == address->sin_port));
	printf("compare IP is %d\n", (current->serv_addr->sin_addr.s_addr == address->sin_addr.s_addr));
	printf("compare complete\n");

	printf("while loop\n");
*/
	//while port and address don't match
	while((current->serv_addr->sin_port != address->sin_port) || (current->serv_addr->sin_addr.s_addr != address->sin_addr.s_addr)){
		//printf("while looping\n");
		current = current->next;
		if (current == NULL){
			//printf("current null\n");
			return 0;
		}
	}

	//printf("bypassing\n");
	//bypass the current link
	current->prev->next = current->next;

	//free currents' inner list

	//printf("removing\n");
	removeAll(current->inner);
	if(current->serv_addr != NULL){
		free(current->serv_addr);
	}
	head->numNodesInList--;
	free(current);
	return 1;
}

int remove_channel(char* data, struct node* head) {
	//start from the first link
	struct node* current = head->next;

	//if list is empty
	if(current == NULL) {
		return 0;
	}

	//navigate through list
	while(strcmp(current->data, data)) {
		//move to next link
		current = current->next;
		if(current == NULL) {
			return 0;
		}
	}

	//bypass the current link
	current->prev->next = current->next;

	//free currents' inner list

	removeAll(current->inner);
	head->numNodesInList--;
	free(current);
	return 1;
}



#endif /* LISTOFLISTS_H_ */
