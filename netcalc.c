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

int base(char *numstr, unsigned int *num)
{
	short base = 10;

	size_t len = strlen(numstr);
	if (len > 1 && numstr[len-1] == 'b') {
		base = 2;
	} else if (len > 2 && numstr[0] == '0' && numstr[1] == 'x') {
		base = 16;
	}

	unsigned long test = strtoul(numstr, NULL, base);
	if (test <= UINT_MAX) {
		*num = (unsigned int) test;
		return base;
	} else {
	       return 0;	
	}
}

int parse(char *buf, size_t buflen, unsigned int *first, unsigned int *second, char *op)
{
	char *num1str = NULL;
	char *num2str = NULL;
	char *opstr = NULL;

	ssize_t num_items = sscanf(buf, "%m[x,0-9,A-F,b]%m[+,-,*,/]%m[x,0-9,A-F,b]", &num1str, &opstr, &num2str);

	int status = 0;
	if (num_items < 3) {
		if (num_items == 1 && base(num1str, first) > 0) {
			status = -2;
		} else {
			status = -1;
		}
	} else if (base(num2str, second) == 0 || base(num1str, first) == 0) {
		status = -1;
	}

	if (opstr) {
		*op = opstr[0];
	}

	free(num1str);
	free(num2str);
	free(opstr);

	return status;
}

void report_error(char *buf, unsigned long buflen, char *msg)
{
	fprintf(stderr, "%s\n", msg);
	memset(buf, 0, buflen);
	sprintf(buf, "%s\n", msg);
}

void bstr(unsigned int n, char **out)
{
	// voodoo
	int msb =  32 - __builtin_clz(n);
	(*out) = (char*) calloc(msb+1, sizeof (char));
	printf("This is ZORK! %d\n", msb);
	(*out)[msb] = '\0';
	for (msb = msb-1; msb >= 0; msb--) {
		(*out)[msb] = (char) (48 + n % 2);
		n = n/2;
	}
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

			if (parse(buf, sizeof buf, &first, &second, &op) < 0) {
				report_error(buf, BUFLEN, "parse: failed to parse");
			} else if (calc(first, second, op, &result) == -1) {
				report_error(buf, BUFLEN, "calc: failed to calculate");
				fprintf(stderr, "debug: %u %c %u\n", first, op, second);
			} else {
				fprintf(stderr, "debug: %u %c %u = %u\n", first, op, second, result);
				// TODO output binary format
				memset(buf, 0, sizeof buf);
				char *bin;
				bstr(result, &bin);
				sprintf(buf, "%u 0x%X %sb\n", result, result, bin);
				free(bin);
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
	char buf[BUFLEN];
	size_t nullsize = 0;
	char *line = NULL;
	while (getline(&line, &nullsize, stdin) != -1) {
		unsigned int num1;
		unsigned int num2;
		char op;
		
		int status = parse(line, sizeof buf, &num1, &num2, &op);
		memset(buf, 0, BUFLEN);

		if (status == -1) {
			report_error(buf, BUFLEN, "parse: failed to parse");
			continue;
		} else if (status == -2) {
			op = '+';
			num2 = 0;
		}

		sprintf(buf, "%u%c%u", num1, op, num2);

		if (send(SOCKFD, buf, strlen(buf), 0) == -1) {
			perror("recv");
			continue;
		}


		free(line);
		line = NULL; // so getline allocates a buffer for us

		memset(buf, 0, BUFLEN);
		ssize_t re_bytes_c = recv(SOCKFD, buf, sizeof buf, 0);
		if (re_bytes_c == -1) {
			perror("recv");
			continue;
		}
		printf("%s", buf);
	}
	return -1;
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
