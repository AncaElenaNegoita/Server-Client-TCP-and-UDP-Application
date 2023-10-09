#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <bits/stdc++.h>

using namespace std;

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1551
#define ID_SIZE 11

typedef struct subscription {
	int poz;
	int sf;
}subscription;

typedef struct client {
	int *active;
	string id;
	vector <string> topic_message_list;
	int* sock;
}client;

typedef struct chat_packet {
	uint16_t len;
	char message[MSG_MAXSIZE + 1];
	int sf;
}chat_packet;

#endif
