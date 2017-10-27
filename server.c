/*
 * server.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 */


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

int main(int argc, char *argv[]){

	return 0;
}
