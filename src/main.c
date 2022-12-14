/* Zachary Perry */
/* Lab A: Threads */
/* 14 December 2022 */
/* Purpose of this lab is to use a combination of sockets and threads to create a chat room server. Clients can join different rooms and communicate with one another in these rooms */
/* NOTE: I did provide and recieve help from other students. We discussed ways to prevent the gradescripts from timing out (closing threads / closing file streams) & error checking */
/* these people were: Manan Patel, Alex Nguyen, Justin Bowers */
/* I also used the threads lecture notes as reference for this lab (particularly threads #3) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "sockettome.h"
#include "jrb.h"
#include "dllist.h"

typedef struct {
	char *name;					/* Name of the room */
	Dllist clients, messages;	/* Dllist to hold clients in the room, Dllist to hold messages to broadcast to the room */
	pthread_mutex_t lock;		/* Mutex lock */
  	pthread_cond_t cv;			/* Condition variable */
} roomInfo;

typedef struct {
	char *name;				/* Name of the client */
	FILE *input, *output;	/* File descriptors for input and output */
	int file_d;				/* File descriptor used for socket_accept_connection */
	roomInfo *room;			/* Pointer to the room this client is currently in */
} clientInfo;

/* Global JRB to hold all chat rooms */
JRB chatRooms;


/**
* @name room_threads
* @brief Will send output (messages) to all clients within the room
* @param[in] void *arg -> roomInfo struct pointer
* @return It will return NULL, but is primarily meant to send output to different client threads 
*/
void *room_threads(void *arg);


/**
* @name client_threads
* @brief Sets up the clients and their information. Allows them to enter input (messages) that will be 
* sent to other clients in the same room 
* @param[in] void *arg -> clientInfo struct pointer
* @return It will return NULL, but is primarily meant to set up each clients and allow them to send messages to others
*/
void *client_threads(void *arg);


int main(int argc, char** argv) {
	int fd;						/* File Descriptor */
	int sock; 					/* Socket Variable */
	roomInfo *room_info;		/* roominfo struct pointer */
	clientInfo *client_info;	/* clientinfo struct pointer */
	pthread_t rid, cid;			/* Variables for room threads & client threads */
	
	/* Error check command line arguments -- Check that proper amount was given */
	if (argc <= 2) {
		fprintf(stderr, "Not enough arguments provided\n");
		exit(1);
	}

	/* Error Check port number -- must be >= 50,000*/
	if (atoi(argv[1]) < 50000) {
		fprintf(stderr, "Port numbers needs to be geater than or equal to 50000\n");
		exit(1);
	}

	/* Create chat room tree */
	chatRooms = make_jrb();

	/* Loop through the room names and begin creating the structs / threads for them */
	/* Assign the name and initialize both the lock and cv */
	/* Then, insert into the tree, keyed on name, and create it's thread -- calling the room_threads function */
	for (int i = 2; i < argc; i++) {
		room_info = (roomInfo *) malloc(sizeof(roomInfo));
		room_info->name = strdup(argv[i]);
		
		pthread_mutex_init(&room_info->lock, NULL);
		pthread_cond_init(&room_info->cv, NULL);

		jrb_insert_str(chatRooms, strdup(room_info->name), new_jval_v((void*)room_info));
		pthread_create(&rid, NULL, room_threads, room_info);
	}

	/* Serve socket using specified port number */
	sock = serve_socket(atoi(argv[1]));

	/* Start creating and accepting client threads */
	/* After accepting the connection to the socket, create a clientInfo struct, assign it's file_d, and create the thread */
	/* Creating the thread will call the client threads function */
	while(1) {
		fd = accept_connection(sock);
		client_info = (clientInfo *) malloc(sizeof(clientInfo));
		client_info->file_d = fd;
		pthread_create(&cid, NULL, client_threads, client_info);
	}

	return 0;
}

void* room_threads(void* arg) {
	roomInfo *ri;			/* roomInfo struct pointer */ 
	clientInfo *ci;			/* clientInfo struct pointer */
	Dllist dnode, dnode2;	/* Dllist traversal iterator nodes */
	char *message;			/* Char to temporarily hold the value within the messages dllist */

	/* Set the roomInfo pointer equal to the struct pointer passed to it */
	/* Initialize both the clients and messages dllist for this room */
	ri = (roomInfo *) arg;
	ri->clients = new_dllist();
	ri->messages = new_dllist();

	/* While loop used to send out messages to all clients within this room */
	while(1) {

		/* If the messages dllist for this room is currently empty, then use the conditon variable to wait */
		/* Once this is signaled, it will move on to the other condition within the while loop */
		if (dll_empty(ri->messages)) pthread_cond_wait(&ri->cv, &ri->lock);

		/* Once the messages dllist is not empty, it will begin traversing it */
		if (!dll_empty(ri->messages)) {
			dll_traverse(dnode, ri->messages) {

				/* Get the current message needed to be sent and traverse all clients within this room */
				message = dnode->val.s;
				dll_traverse(dnode2, ri->clients) {
					ci = dnode2->val.v;

					/* Used fputs and fflush to write the message to each clients output stream -> error checks for EOF */
					/* If it is EOF (for either fputs or fflush) it will close the output stream and delete the message node */
					if (fputs(message, ci->output) == EOF) {
						fclose(ci->output);
						dll_delete_node(dnode);
					}
					else if (fflush(ci->output) == EOF) {
						fclose(ci->output);
						dll_delete_node(dnode);
					}
				}
				/* Delete the message from the dllist and break */
				dll_delete_node(dnode);
				break;
			}
		}
	}
	return NULL;
}


void* client_threads(void* arg){
	clientInfo *ci, *ci2;		/* clientInfo struct pointers */
	roomInfo *ri;				/* roomInfo struct pointers */
	Dllist dnode;				/* Node for traversing dllist */
	JRB node;					/* Node for traversing JRB */
	char clientName[500];		/* Buffer for input client name */
	char roomName[500];			/* Buffer for input room name */
	char inputBuffer[500];		/* Buffer for client message input */
	char inputSender[500];		/* Buffer for full string to send to other clients */
	int foundRoom = 0;			/* Tracker, used to see if the entered room exists */

	/* Set the clientInfo pointer equal to the struct the function was passed */
	/* Initialize the input and output streams for this client -- use fdopen on the saved accept_connection file_d to open them for read / write */
	ci = (clientInfo *) arg;
	ci->input = fdopen(ci->file_d, "r");
	ci->output = fdopen(ci->file_d, "w");

	/* Use fputs to output the prompt to the client */
	/* ONE NOTE: All fputs, fflush, and fgets calls all error check and will close the file streams / kill the thread if true */
	if (fputs("Chat Rooms:\n\n", ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	/* Traverse each room, output their name, and all of their clients to the current client */
	jrb_traverse(node, chatRooms) {
		ri = (roomInfo *)node->val.v;

		/* Outputting room name */
		if (fputs(jval_s(node->key), ci->output) == EOF) {
			fclose(ci->input);
			fclose(ci->output);
			pthread_exit(NULL);
		}

		if (fputs(":", ci->output) == EOF) {
			fclose(ci->input);
			fclose(ci->output);
			pthread_exit(NULL);
		}

		/* Traversing the rooms client list (if not empty) and outputting the name of each client */
		if (!dll_empty(ri->clients)) {
			dll_traverse(dnode, ri->clients) {
				ci2 = (clientInfo *)dnode->val.v;
				if (fputs(" ", ci->output) == EOF) {
					fclose(ci->input);
					fclose(ci->output);
					pthread_exit(NULL);
				}

				if (fputs(ci2->name, ci->output) == EOF) {
					fclose(ci->input);
					fclose(ci->output);
					pthread_exit(NULL);
				}
			}
		}
		if (fputs("\n", ci->output) == EOF) {
			fclose(ci->input);
			fclose(ci->output);
			pthread_exit(NULL);
		}
	}

	/* Fflush call to send all out above ^ to the client */
	if (fflush(ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}
	
	/* Prompt the client for their name & save their input using fgets */
	if (fputs("\nEnter your chat name (no spaces):\n", ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	if (fflush(ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}
	if (fgets(clientName, sizeof(clientName), ci->input) == NULL) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	/* Prompt the client for the room they would like to enter & save the roomName */
	if (fputs("Enter chat room:\n", ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	if (fflush(ci->output) == EOF) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	if (fgets(roomName, sizeof(roomName), ci->input) == NULL) {
		fclose(ci->input);
		fclose(ci->output);
		pthread_exit(NULL);
	}

	/* Since these are buffers, null out the last index of both the clientName and roomName variables */
	/* Also save the client name entered into the current client's struct */
	clientName[strlen(clientName) - 1] = '\0';
	roomName[strlen(roomName) - 1] = '\0';
	ci->name = clientName;

	/* Traverse the chat rooms tree in order to find out if the entered room name exists */
	/* If it does, have the client "join" */
	jrb_traverse(node, chatRooms) {
		ri = (roomInfo *)node->val.v;

		/* Compare each key with the room name to see if the room exists */
		if (strncmp(jval_s(node->key), roomName, strlen(jval_s(node->key))) == 0) {
			
			/* If so, assign the clients roomInfo pointer to point to this room */
			ci->room = ri;

			/* Lock the rooms mutex & append the current client to the rooms client dllist */
			pthread_mutex_lock(&ri->lock);
			dll_append(ri->clients, new_jval_v((void *)ci));
			
			/* Construct the notification message to send out to all current clients within this room */
			/* Append the message to the rooms messages dllist, signal the CV in the rooms thread function, and unlock the mutex */
			strcpy(inputSender, ci->name);
			strcat(inputSender, " has joined\n");
			dll_append(ci->room->messages, new_jval_s(strdup(inputSender)));

			pthread_cond_signal(&ri->cv);
			pthread_mutex_unlock(&ri->lock);

			/* Flip the tracker and break, since the room was found */
			foundRoom = 1;
			break;
		}
	}

	/* If the room was not found, print an error message and exit */
	if (foundRoom == 0) {		
		fprintf(stderr, "An invalid room name was entered...\n");
        exit(1);
	}

	/* While loop for entered client input */
	while(1) {

		/* If the client has entered valid input */
		if (fgets(inputBuffer, sizeof(inputBuffer), ci->input) != NULL) {

			/* Construct the formatted output (name: message)*/
			strcpy(inputSender, ci->name);
			strcat(inputSender, ": ");
			strcat(inputSender, inputBuffer);
			
			/* Lock the rooms mutex, append the message to the rooms messages dllist, signal the CV, and unlock */
			/* Doing so will run the while loop in the roomThread function, outputting the message to all clients in the room */
			pthread_mutex_lock(&ci->room->lock);
			dll_append(ci->room->messages, new_jval_s(strdup(inputSender)));
			pthread_cond_signal(&ci->room->cv);
			pthread_mutex_unlock(&ci->room->lock);
		}
		
		/* If the client has left the room (CTRL C or D)*/
		else if (fgets(inputBuffer, sizeof(inputBuffer), ci->input) == NULL) {
			
			/* Close the clients input & output streams */
			fclose(ci->input);
			fclose(ci->output);
			/* Construct the notification for the user leaving the room */
			strcpy(inputSender, ci->name);
			strcat(inputSender, " has left\n");

			/* Lock the rooms mutex, append the notification the messages dllist, and signal the CV for the room */
			pthread_mutex_lock(&ci->room->lock);
			dll_append(ci->room->messages, new_jval_s(strdup(inputSender)));
			pthread_cond_signal(&ci->room->cv);

			/* Traverse the rooms client list until you find this client -- remove them from the list */
			dll_traverse(dnode, ci->room->clients) {
				ci2 = dnode->val.v;
				if (strcmp(ci2->name, ci->name) == 0) {
					dll_delete_node(dnode);
					break;
				}
			}

			/* Lastly, unlock the mutex and call pthread_exit to make the thread itself exit */
			pthread_mutex_unlock(&ci->room->lock);
			pthread_exit(NULL);
		}
	}
	return NULL;
}