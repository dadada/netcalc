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
#include <limits.h>

#define BUFLEN 512

int SOCKFD;

int CLIENT;

void cleanup(int signum)
{
	if (close(SOCKFD) != 0) {
		perror("close");
	}
}

int prepaddr(char *host, char *port, struct addrinfo **ainfo)
{
	fprintf(stderr, "debug: %s %s\n", host, port);
	struct addrinfo hints; // hints to what we want
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status;
	if ((status = getaddrinfo(host, port, &hints, ainfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}

	return 0;
}

int bindsocket(int sockd, struct addrinfo *ainfo)
{
	if (bind(SOCKFD, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
		perror("bind");
		cleanup(0);
		return -1;
	}

	if (listen(SOCKFD, 5) != 0) {
		perror("listen");
		cleanup(0);
		return -1;
	}
	return sockd;
}

int prepsocket(struct addrinfo *ainfo)
{
	SOCKFD = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);

	if (SOCKFD == -1) {
		perror("socket");
		return -1;
	}

	int yes = 1;
	if (setsockopt(SOCKFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
		perror("setsockopt");
		cleanup(0);
		return -1;
	}

	return SOCKFD;
}

int readnum(char *buf, size_t buflen, unsigned int *res)
{
	size_t bytes_read = 0;
	char *p = buf;

	while (bytes_read < buflen && *p!='+' && *p!='+'&& *p!='-'&& *p!='*'&& *p!='/' && *p!='\n') {
		bytes_read++;
		p++;
	}

	int bytes_to_copy = bytes_read;
	int base = 10;

	if (bytes_read > 2 && buf[0] == '0' && buf[1] == 'x') {
		base = 16;
	} else if (bytes_read > 1 && buf[bytes_read-1] == 'b') {
		base = 2;
		bytes_to_copy--;
	}

	char num[35];
	strncpy(num, buf, bytes_to_copy);
	num[bytes_to_copy] = '\0';

	unsigned long parsed = strtoul(num, NULL, base);
	if (parsed == ULONG_MAX) {
		perror("strtoul");
		return -1;
	}

	*res = (unsigned int) parsed;

	return bytes_read;
}

int calc(unsigned int num1, unsigned int num2, char op, unsigned int *result)
{
	switch (op) {
		case '+':
			if (__builtin_uadd_overflow(num1, num2, result)) {
				return -1;
			}
			break;
		case '-':
			if (__builtin_usub_overflow(num1, num2, result)) {
				return -1;
			}
			break;
		case '*':
			if (__builtin_umul_overflow(num1, num2, result)) {
				return -1;
			}
			break;
		case '/':
			if (num1 == 0 || num2 == 0) {
				return -1;
			}
			*result = num1 / num2;
			break;
		default:
			return -1;
	}
	return 0;
}

int parse(char *buf, size_t buflen, unsigned int *first, unsigned int *second, char *op)
{
	//read first number

	size_t read = readnum(buf, buflen, first);
        if (read == -1) {
		return -1;
	}

	// read operator
	if (read < buflen && buf[read] != '\n' && buf[read] != '\0') {
		*op = buf[read];
		read++;
	}

	// read second number
        if (readnum(buf+read, buflen-read, second) == -1) {
		return -1;
	}

	return 0;
}

void report_error(char *buf, unsigned long buflen, char *msg)
{
	fprintf(stderr, "%s\n", msg);
	memset(buf, 0, buflen);
	sprintf(buf, "%s\n", msg);
}

int server()
{
	while (1) {
		struct sockaddr_storage c_addr;
		socklen_t sin_size = sizeof c_addr;
		int c_fd = accept(SOCKFD, (struct sockaddr *)&c_addr, &sin_size);

		if (c_fd == -1) {
			perror("accept");
			continue; // we do not care
		}

		fprintf(stderr, "debug: connected\n");

		char buf[BUFLEN];
		
		ssize_t byte_count;
		while ((byte_count = recv(c_fd, buf, sizeof buf, 0)) != 0) {
			if (byte_count == -1) {
				perror("recv");
				break;
			}

			unsigned int first;
			unsigned int second;
			char op;
			unsigned int result;

			if (parse(buf, sizeof buf, &first, &second, &op) == -1) {
				report_error(buf, BUFLEN, "parse: failed to parse");
			} else if (calc(first, second, op, &result) == -1) {
				report_error(buf, BUFLEN, "calc: failed to calculate");
				fprintf(stderr, "debug: %u %c %u\n", first, op, second);
			} else {
				fprintf(stderr, "debug: %u %c %u = %u\n", first, op, second, result);
				// TODO output binary format
				memset(buf, 0, sizeof buf);
				sprintf(buf, "%u 0x%X \n", result, result);
			}

			if (send(c_fd, buf, sizeof buf, 0) == -1) {
				perror("send");
				continue;
			}
		}
		close(c_fd);
	}
}

int connectclient(struct addrinfo *ainfo)
{
	if (connect(SOCKFD, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
		perror("connect");
		return -1;
	}
	return 0;
}

int client()
{
	char sendbuf[512];
	char *num1 = NULL;
	char *num2 = NULL;
	char *op = NULL;
	while (scanf("%m[0-9,A-F,x,b]%m[+,-,*,/]%m[0-9,A-F,x,b]", &num1, &op, &num2)) {
		//fprintf(stdout, "%s %s %s\n", num1, op, num2);
		free(num1);
		free(num2);
		free(op);
	}
}

int main(int argc, char *argv[])
{
	char* host = "localhost";
	char* port = "5000";

	if (argc > 1) {
		if (strcmp(argv[1], "-c") == 0) {
			printf("debug: argv[1] = %s running in client mode\n", argv[1]);
			CLIENT = 1; // client mode
			if (argc > 3) {
				host = argv[2];
				port = argv[3];
			}
		} else {
			printf("debug: argv[1] = %s running in server mode\n", argv[1]);
			CLIENT = 0;
			if (argc > 2) {
				host = NULL;
				port = argv[1];
			}
		}
	}

	struct addrinfo *ainfo;
	if (prepaddr(host, port, &ainfo) != 0) {
		fprintf(stderr, "prepaddr: failed to obtain adress.\n");
		return 1;
	}

	if ((SOCKFD = prepsocket(ainfo)) == -1) {
		return 1;
	}

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = cleanup;
	sigaction(SIGTERM, &action, NULL);

	if (CLIENT) {
		connectclient(ainfo);
		if (client() == -1) {
			fprintf(stderr, "client: error");
		}
	} else {
		if ((SOCKFD = bindsocket(SOCKFD, ainfo)) == -1) {
			fprintf(stderr, "bindsocket: failed to bind or listen.");
			return 1;
		}
		if (server() == -1) {
			fprintf(stderr, "server: error");
		}
	}

	cleanup(0);
	freeaddrinfo(ainfo); // free the linked list
	return 0;
}
