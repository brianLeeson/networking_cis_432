/*
 * client.c
 *
 *  Created on: Oct 26, 2017
 *      Author: brian
 */


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

	return 0;
}
