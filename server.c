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

	dll_channels = initDLL();
	dll_users = initDLL();

	//make sockfd

	//bind sockfd

	//while true
	while(){
		//recvfrom sockfd

		//cast generic

		//get type

		//cast type

		//if <type>
			//action

	}
	printf("server exiting\n");
	return 0;
}
