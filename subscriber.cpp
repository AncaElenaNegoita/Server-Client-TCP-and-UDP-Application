#include <bits/stdc++.h>
#include <vector>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "common.h"
#include "helpers.h"
#include "poll.h"

#define MAX_PFDS 10000

using namespace std;

void run_client(int sockfd, char* id) {
	string command;

	chat_packet sent_packet;
	chat_packet recv_packet;

	/* Multiplexes between reading from the keyboard and receiving one
	 message, so that order is no longer imposed. */
	struct pollfd pfds[MAX_PFDS];
	int nfds = 0;
	
	pfds[nfds].fd = STDIN_FILENO;
	pfds[nfds++].events = POLLIN;
	
	/* Create the server socket. */
	pfds[nfds].fd = sockfd;
	pfds[nfds++].events = POLLIN;
	
	while (1) {
		/* Wait until data is received from at least one file descriptors. */
		poll(pfds, nfds, -1);

		if ((pfds[1].revents & POLLIN)) {
			/* New message. */
			recv_all(sockfd, &recv_packet, sizeof(recv_packet));
			/* If it receives exit, close the current socket. */
			if (strncmp(recv_packet.message, "Close!", sizeof("Close!") - 1) == 0) {
				close(sockfd);
				return;
			} else {
				/* Show the message(from the topic). */
				cout << recv_packet.message;
			}
		} else if ((pfds[0].revents & POLLIN)) {
			memset(&sent_packet, 0, sizeof(chat_packet));
			/* Reads data from user. */
			cin >> command;
			if (command == "exit") {
				/* Close the socket. */
				close(sockfd);
				return;
			} else if (command == "subscribe") {
				/* Sent a packet to the server to inform about the
				 subscription. */
				int sf;
				string topic;
				cin >> topic >> sf;
				DIE((sf != 0 && sf != 1), "Not a valid sf");

				/* Insert the sf and topic in the packet. */
				memset(&sent_packet, 0, sizeof(sent_packet));
				sent_packet.len = strlen(topic.c_str()) + 1;
				sent_packet.sf = sf;
				strcpy(sent_packet.message, topic.c_str());

				/* Use send_all function to send the packet to the
				 server. */
				send_all(sockfd, &sent_packet, sizeof(sent_packet));
				cout << "Subscribed to topic.\n";
			} else if (command == "unsubscribe") {
				/* Sent a packet to the server to inform about the
				 unsubscription. */
				char buf[BUFSIZ];
				string topic;
				cin >> topic;
				memset(&sent_packet, 0, sizeof(sent_packet));
				sent_packet.sf = 2;
				sent_packet.len = strlen(topic.c_str()) + 1;
				strcpy(sent_packet.message, topic.c_str());

				/* Use send_all function to send the packet to the
				 server. */
				send_all(sockfd, &sent_packet, sizeof(sent_packet));
				printf("Unsubscribed from topic.\n");	
			} else {
				//DIE(1, "Command unknown");
				continue;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int sockfd = -1;

	if (argc != 4) {
		printf("Usage: %s client_id server_address server_port\n", argv[0]);
		return 1;
	}

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	/* Parse port as a number. */
	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	/* Obtain a TCP socket for connecting to the server. */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	/* Insert in serv_addr the address, family and port in order to connect. */
	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	/* Connect to the server. */
	rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "connect");

	/* Send the ID to the server. */
	chat_packet send_id;
	send_id.len = strlen(argv[1]) + 1;
	strcpy(send_id.message, argv[1]);
	send_all(sockfd, &send_id, sizeof(send_id));

	run_client(sockfd, argv[1]);

	return 0;
}
