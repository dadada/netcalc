#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int cleanup(int sockfd)
{
	if (close(sockfd) != 0) {
		perror("listen");
		return -1;
	}

	return 0;
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

	int sockfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (sockfd == -1) {
		perror("socket");
		return -1;
	}

	if (bind(sockfd, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
		perror("bind");
		cleanup(sockfd);
		return -1;
	}

	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		cleanup(sockfd);
		return -1;
	} 

	if (listen(sockfd, 5) != 0) {
		perror("listen");
		cleanup(sockfd);
		return -1;
	}
	return sockfd;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr,"usage: netcalc hostname port\n");
		return 1;
	}

	struct addrinfo *ainfo;
	if (prepaddr(argv[1], argv[2], &ainfo) != 0) {
		fprintf(stderr, "server: failed to obtain adress.");
		return 1;
	}

	printaddr(ainfo);

	int sockfd;
	if ((sockfd = prepsocket(ainfo)) == -1) {
		return 1;
	}

	while (1) {
		struct sockaddr_storage c_addr;
		socklen_t sin_size = sizeof c_addr;
		int c_fd = accept(sockfd, (struct sockaddr *)&c_addr, &sin_size);
		if (c_fd == -1) {
			perror("accept");
			continue; // we do not care
		}
		if (send(c_fd, "Hello world!\n", 14, 0) == -1) {
			perror("send");
		}
		close(c_fd);
	}

	cleanup(sockfd);
	freeaddrinfo(ainfo); // free the linked list
	return 0;
}
