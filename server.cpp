#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 10000

using namespace std;

void shut_down_server(vector <client>& clients) {
    /* Send shutdown message to each connected client. */
    for (client c : clients) {
        /* Create the message. */
        chat_packet message;
        memset(&message, 0, sizeof(message));
		message.len = sizeof("Close!");
		strcpy(message.message, "Close!");

		if (*c.active == 1) {
			/* Send the message. */
			int bytes_sent = send_all(*(c.sock), &message, sizeof(message));
			if (bytes_sent < 0) {
				perror("Failed to send shutdown message.");
			}

			/* Close the socket and remove from fd set. */
			close(*c.sock);
		}
    }
}

void transform_datagram(char *m, struct sockaddr_in& udp_addr,
						unordered_map <string, vector<subscription>> &topics,
						vector <client> &clients) {
	string s;
	/* Store topic. */
    char topic[51];
	strncpy(topic, m, 50);

	/* Store type. */
	int data_type = m[50];

	/* Store contents. */
	char contents[1501];
	memcpy(&contents, &m[51], sizeof(contents));

    chat_packet sent_packet;
	/* Store the ip and the port. */
	char *ip = inet_ntoa(udp_addr.sin_addr);
	int port = htons(udp_addr.sin_port);

    switch (data_type) {
		/* Signed 32-bit integer. */
        case 0: {
            s = "INT";
			uint8_t sign = contents[0];
			int number = ntohl(*(uint32_t *)((void *)(m + 52)));
			if (sign == 1) {
				number *= -1;
			}
			memset(&contents, 0, sizeof(contents));
			strcpy(contents, to_string(number).c_str());
            break;
        }
		/* 16-bit floating point. */
        case 1: {
            s = "SHORT_REAL";
			double float_number = ntohs(*(uint32_t *)(void *)(m + 51));
        	float_number /= 100;
			memset(&contents, 0, sizeof(contents));
			stringstream ss;
    		ss << fixed << setprecision(2) << float_number;
    		string result = ss.str();
			strcpy(contents, result.c_str());
            break;
        }
		/* Signed 32-bit floating point. */
        case 2: {
            s = "FLOAT";
			uint8_t sign = contents[0];
			double float_number = ntohl(*(uint32_t *)(void *)(m + 52));
			uint8_t power = contents[5];
			for (int i = 0; i < power; i++) {
				float_number /= 10.0;
			}
			if (sign == 1) {
				float_number *= (-1);
			}
			memset(&contents, 0, sizeof(contents));
			stringstream ss;
    		ss << fixed << setprecision(power) << float_number;
    		string result = ss.str();
			strcpy(contents, result.c_str());
            break;
        }
		/* String. */
        case 3: {
            s = "STRING";
            break;
        }
		/* Undefined type. */
        default: {
            DIE(1, "Unknown UDP datagram type");
        }
    }
	char buf[MSG_MAXSIZE + 1];
	strcpy(buf, ip);
	strcat(buf, ":");
	strcat(buf, to_string(port).c_str());
	strcat(buf, " - ");
	strcat(buf, topic);
	strcat(buf, " - ");
	strcat(buf, s.c_str());
	strcat(buf, " - ");
	strcat(buf, contents);
	strcat(buf, "\n");
	
	memset(&sent_packet, 0, sizeof(sent_packet));
	strcpy(sent_packet.message, buf);
	sent_packet.len = strlen(buf) + 1;

	
	for (auto& p : topics[topic]) {
		if (*clients[p.poz].active == 1) {
			send_all(*clients[p.poz].sock, &sent_packet, sizeof(sent_packet));
		} else {
			if (p.sf == 1) {
				clients[p.poz].topic_message_list.push_back(string(buf));
			}
		}
	}
}

void run_chat_multi_server(int listenfd, int udp_sockfd, vector <client> &clients,
						   unordered_map <string, vector<subscription>> &topics,
						   struct sockaddr_in& udp_addr) {
	char buf[BUFSIZ];
	struct pollfd poll_fds[MAX_CONNECTIONS];
	int nclients = 0;
	int rc;

	struct chat_packet received_packet;

	/* Set socket listenfd. */
	rc = listen(listenfd, MAX_CONNECTIONS);
	DIE(rc < 0, "listen");

	/* Add the file descriptors. */
	poll_fds[nclients].fd = STDIN_FILENO;
	poll_fds[nclients++].events = POLLIN;

	poll_fds[nclients].fd = listenfd;
	poll_fds[nclients++].events = POLLIN;

	poll_fds[nclients].fd = udp_sockfd;
	poll_fds[nclients++].events = POLLIN;

	while (1) {
		rc = poll(poll_fds, nclients, -1);
		DIE(rc < 0, "poll");

		for (int i = 0; i < nclients; i++) {
			chat_packet received_packet;
			memset(&received_packet, 0, sizeof(received_packet));
			if (poll_fds[i].revents & POLLIN) {
				/* Read from STDIN. */
				if (poll_fds[i].fd == STDIN_FILENO) {
					memset(&buf, 0, sizeof(buf));
					int rc = read(0, buf, sizeof(buf) - 1);
					DIE(rc < 0, "server read from stdin");

					/* Close all connections. */
					if (strncmp(buf, "exit", 4) == 0) {
						shut_down_server(clients);
						return;
					}
					break;
				} else if (poll_fds[i].fd == listenfd) {
					/* A client is trying to connect through the socket. */
					/* Request to connect on a given socket. */
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					int newsockfd = accept(listenfd,
									(struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");
					int rc = recv_all(newsockfd, &received_packet,
									  sizeof(received_packet));
					DIE(rc < 0, "recv");

					/* Search for a client with the same ID and already connected. */
					auto it = find_if(clients.begin(), clients.end(), [&](const client& c) {
						return (c.id == string(received_packet.message) && *c.active == 1);
					});

					/* Search for a client with the same ID.*/
					auto id = find_if(clients.begin(), clients.end(), [&](const client& c) {
						return (c.id == string(received_packet.message));
					});

					/* Verify if the client is already connected. */
					if (it != clients.end()) {
						/* Print a message indicating that the client is already connected. */
						cout << "Client " << string(it->id).c_str() << " already connected.\n";
						
						/* Clear the contents of the received message (we don't care about its
						contents at this point). */
						memset(&received_packet, 0, sizeof(received_packet));
						
						chat_packet message;
						memset(&message, 0, sizeof(message));
						message.len = sizeof("Close!");
						strcpy(message.message, "Close!");

						send_all(newsockfd, &message, sizeof(message));

						/* Close the socket. */
						close(newsockfd);
						break;
					} else {
						/* Verify if the client already exists and it's disconnected. */
						if (id != clients.end()) {
							*(id->active) = 1;
							*(id->sock) = newsockfd;
							cout << "New client " << id->id << " connected from port "
								 << ntohs(cli_addr.sin_port) <<".\n";

							/* Take each message that was sent offline and sent
							 it to the client. */
							for (auto s : id->topic_message_list) {
								chat_packet message;
								memset(&message, 0, sizeof(message));
								message.len = sizeof(s.c_str() + 1);
								strcpy(message.message, s.c_str());

								send_all(newsockfd, &message, sizeof(message));
							}
							/* Add the new socket in the descriptors list. */
							poll_fds[nclients].fd = newsockfd;
							poll_fds[nclients++].events = POLLIN;

							/* Empty the list of messages. */
							id->topic_message_list.clear();
						} else {
							/* Add the new socket in the descriptors list. */
							poll_fds[nclients].fd = newsockfd;
							poll_fds[nclients++].events = POLLIN;

							/* Add a new client in the list. */
							client c;
							c.id = received_packet.message;
							c.active = new int;
							*(c.active) = 1;
							c.topic_message_list = {};
							c.sock = new int;
							*(c.sock) = newsockfd;
							clients.push_back(c);

							cout << "New client " << c.id << " connected from port "
								 << ntohs(cli_addr.sin_port) <<".\n";
							break;
						}
					}
					break;
				/* If the UDP socket is open. */
				} else if (poll_fds[i].fd == udp_sockfd) {
					char *m = (char *)malloc(1600);
					recvfrom(udp_sockfd, (char *)m, 1600, 0,
							(struct sockaddr *)&udp_addr,
							(socklen_t *)sizeof(udp_addr));
					/* Transform the data in order to be readable by TCP. */
					transform_datagram(m, udp_addr, topics, clients);
					break;
				} else {
					/* Server received data from one of the sockets. */
					int rc = recv_all(poll_fds[i].fd, &received_packet,
									  sizeof(received_packet));
					DIE(rc < 0, "recv");

					/* Search the client by the socket. */
					auto id = find_if(clients.begin(), clients.end(),
									  [&](const client& c) {
						return (*c.sock == poll_fds[i].fd);
					});

					if (rc == 0) {
						/* Close the connection. */
						cout << "Client " << id->id << " disconnected.\n";
						close(poll_fds[i].fd);

						*(id->active) = 0;

						/* Eliminate the closed socket. */
						for (int j = i; j < nclients - 1; j++) {
							poll_fds[j] = poll_fds[j + 1];
						}
						nclients--;
					} else {
						int poz = id - clients.begin();
						/* Verify if the command was subscribe or unsubscribe. */
						if (received_packet.sf == 1 || received_packet.sf == 0) {
							/* If it was subscribe, save the position of the client
							 and save sf. */
							subscription p;
							p.poz = poz;
							p.sf = received_packet.sf;
							topics[received_packet.message].push_back(p);
						} else {
							/* If it was unsubscribe, search for the position in
							 the vector of clients and erase the entry. */
							int index = 0;
							for (auto& sub : topics[received_packet.message]) {
								if (sub.poz == poz) {
									topics[received_packet.message]
									.erase(topics[received_packet.message].begin() + 
										   index);
									break;
								}
								index++;
							}
						}
					}
					break;
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s server_port\n", argv[0]);
		return 1;
	}

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	/* Parse the port as a number. */
	uint16_t port;
	int rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	/* Map that stores the disconnected clients. */
	unordered_map <string, vector<subscription>> topics;

	/* Vector that stores the clients. */
	vector <client> clients;

	/* Obtain the TCP socket for connection. */
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenfd < 0, "socket");

	/* Add the address, family and port for connecting. */
	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	/* Set reusable and no delay. */
	int enable = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

	/* Associate the address of the server with the TCP socket. */
	rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");

	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	/* Add the address, family and port for connecting. */
	struct sockaddr_in serv_udp_addr;
	memset(&serv_udp_addr, 0, socket_len);
	serv_udp_addr.sin_family = AF_INET;
	serv_udp_addr.sin_port = htons(port);
	serv_udp_addr.sin_addr.s_addr = INADDR_ANY;

	/* Associate the address of the server with the UDP socket. */
	rc = bind(udp_sockfd, (const struct sockaddr *) &serv_udp_addr, sizeof(struct sockaddr));
	DIE(rc < 0, "UDP bind error");

	run_chat_multi_server(listenfd, udp_sockfd, clients, topics, serv_udp_addr);

	/* Close listenfd and udp_sockfd. */
	close(listenfd);
	close(udp_sockfd);

	return 0;
}
