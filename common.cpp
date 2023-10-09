#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

using namespace std;

/* This function receives len bytes from the buffer. */
int recv_all(int sockfd, void *buffer, size_t len) {

	size_t bytes_received = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;

	while(bytes_remaining) {
		size_t n = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
		if (n < 0) {
			return -1;
		} else if (n == 0) {
			return 0;
		}
			bytes_received += n;
			bytes_remaining -= n;
	}

	return recv(sockfd, buffer, len, 0);
}

/* This function sends len bytes from the buffer. */
int send_all(int sockfd, void *buffer, size_t len) {
	size_t bytes_sent = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;
	
	while(bytes_remaining) {
		size_t n = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
		if (n < 0) {
			return -1;
		} else if (n == 0) {
			return 0;
		}
		bytes_sent += n;
		bytes_remaining -= n;
	}
	return send(sockfd, buffer, len, 0);
}