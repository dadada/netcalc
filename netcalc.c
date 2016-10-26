#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

#define BUFLEN 67

int sockfd;

void cleanup(int signum)
{
	if (close(sockfd) != 0) {
		perror("close");
	}
}

int prepaddr(char *host, char *port, struct addrinfo **ainfo)
{
	struct addrinfo hints; // hints to what we want
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status;
	if ((status = getaddrinfo(host, port, &hints, ainfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}
	return 0;
}

void printaddr(struct addrinfo *ainfo)
{

	char ipstr[INET6_ADDRSTRLEN]; // will hold the IP adress, humanreadable
	void *addr;
	char *ipver;
	if (ainfo->ai_family == AF_INET) { // IPv4
		struct sockaddr_in *ipv4 = (struct sockaddr_in *) ainfo->ai_addr;
		addr = &(ipv4->sin_addr);
		ipver = "IPv4";
	} else { // IPv6
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) ainfo->ai_addr;
		addr = &(ipv6->sin6_addr);
		ipver = "IPv6";
	}
	inet_ntop(ainfo->ai_family, addr, ipstr, sizeof ipstr);
	printf("Listening on  %s: %s\n", ipver, ipstr);
}

int prepsocket(struct addrinfo *ainfo)
{

	sockfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (sockfd == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		cleanup(0);
		return -1;
	}

	if (bind(sockfd, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
		perror("bind");
		cleanup(0);
		return -1;
	}

	if (listen(sockfd, 5) != 0) {
		perror("listen");
		cleanup(0);
		return -1;
	}
	return sockfd;
}

int server()
{
	while (1) {
		struct sockaddr_storage c_addr;
		socklen_t sin_size = sizeof c_addr;
		int c_fd = accept(sockfd, (struct sockaddr *)&c_addr, &sin_size);

		if (c_fd == -1) {
			perror("accept");
			continue; // we do not care
		}
		
		char buf[BUFLEN];
		
		ssize_t byte_count = recv(c_fd, buf, sizeof buf, 0);
		if (byte_count == -1) {
			perror("recv");
			continue;
		}

		printf("received %ld bytes containig %s", byte_count, buf);

		//calc(&buf, strlen(buf));

		if (send(c_fd, buf, strlen(buf), 0) == -1) {
			perror("send");
			continue;
		}

		close(c_fd);
	}
}

int main(int argc, char *argv[])
{
	char* host = "localhost";
	char* port = "5000";

	if (argc > 1) {
		port = argv[1];
	}

	struct addrinfo *ainfo;
	if (prepaddr(host, port, &ainfo) != 0) {
		fprintf(stderr, "server: failed to obtain adress.");
		return 1;
	}

	printaddr(ainfo);

	if ((sockfd = prepsocket(ainfo)) == -1) {
		return 1;
	}

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = cleanup;
	sigaction(SIGTERM, &action, NULL);

	server();

	cleanup(0);
	freeaddrinfo(ainfo); // free the linked list
	return 0;
}
