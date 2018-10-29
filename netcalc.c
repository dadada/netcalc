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

/// closes socket, signal handler
/// @param signum
void cleanup(int signum) {
    if (close(SOCKFD) != 0) {
        perror("close");
    }
    exit(1);
}

/// Fills addrinfo with given params
/// @param host
/// @param port to bind
/// @param ainfo the addrinfo struct to fill
/// @param afamily specifies protocol family
/// @param flags
/// @return 0 if success, -1 if stderr
int prepaddr(char *host, char *port, struct addrinfo **ainfo, int afamily, int flags) {
    struct addrinfo hints; // hints to what we want
    memset(&hints, 0, sizeof hints);
    hints.ai_family = afamily; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = flags;

    int status;
    if ((status = getaddrinfo(host, port, &hints, ainfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    return 0;
}

/// Binds to sockd
/// @param sockd the socket to bind to
/// @param ainfo information for binding
/// @return sockd freshly bound socket or -1 if error
int bindsocket(int sockd, struct addrinfo *ainfo) {
    if (bind(sockd, ainfo->ai_addr, ainfo->ai_addrlen) != 0) {
        perror("bind");
        cleanup(0);
        return -1;
    }

    if (listen(sockd, 5) != 0) {
        perror("listen");
        cleanup(0);
        return -1;
    }
    return sockd;
}

/// Prepares SOCKFD socket for further use
/// @param ainfo addrinfo struct to configure SOCKFD
/// @returns SOCKFD if successfull
int prepsocket(struct addrinfo *ainfo) {
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

/// Does the calculation
/// @param num1 first operand
/// @param num2 second operand
/// @param op operator
/// @param result stores the result of the calculation
/// @return 0 on success, -1 on failure
int calc(unsigned int num1, unsigned int num2, char op, unsigned int *result) {
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

/// Converts a string into an integer using its base
/// @param numstr input string
/// @param num stores the decoded integer
/// @return 0 on failure, the encoded base of the integer on success
int base(char *numstr, unsigned int *num) {
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

/// Parses the client input into a command
/// @param buf source buffer
/// @param buflen length of source buffer
/// @param first first operand
/// @param second second operand
/// @param op operator (+,-,*,/)
/// @return 0 on success -1 on parser error, -2 on invalid values
int parse(char *buf, size_t buflen, unsigned int *first, unsigned int *second, char *op) {
    char *num1str = NULL;
    char *num2str = NULL;
    char *opstr = NULL;



    ssize_t num_items = sscanf(buf, "%m[x0-9A-Fb]%m[+-*/]%m[x0-9A-Fb]", &num1str, &opstr, &num2str);

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

/// Prints a formatted error to stderr and a buffer
/// @param buf destination buffer
/// @param buflen length of buf
/// @param msg error message
void report_error(char *buf, unsigned long buflen, char *msg) {
    fprintf(stderr, "%s\n", msg);
    memset(buf, 0, buflen);
    sprintf(buf, "%s\n", msg);
}

/// Converts an integer into a string representation of its binary encoding
/// @param n an integer
/// @param out stores the resulting string
void bstr(unsigned int n, char **out) {
    // voodoo
    int msb =  32 - __builtin_clz(n);
    (*out) = (char*) calloc(msb+1, sizeof (char));
    (*out)[msb] = '\0';
    for (msb = msb-1; msb >= 0; msb--) {
        (*out)[msb] = (char) (48 + n % 2);
        n = n/2;
    }
}

/// Starts a server that listens for commands and performs calculations
/// @return 0 on success <0 on failure
int server() {
    struct sockaddr_storage c_addr;
    socklen_t sin_size = sizeof c_addr;
    while (1) {
        int c_fd = accept(SOCKFD, (struct sockaddr *)&c_addr, &sin_size);

        if (c_fd == -1) {
            perror("accept");
            continue; // we do not care
        }

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
            } else {
                memset(buf, 0, sizeof buf);
                char *bin;
                bstr(result, &bin);
                sprintf(buf, "%u 0x%X %sb\n", result, result, bin);
                free(bin);
            }

            if (send(c_fd, buf, strlen(buf)+1, 0) == -1) {
                perror("send");
                continue;
            }
        }
        close(c_fd);
    }
    return 0;
}

/// Connects the client to the server
/// @return 0 on success <0 on failure
int connectclient(struct addrinfo *ainfo) {
    if (connect(SOCKFD, ainfo->ai_addr, ainfo->ai_addrlen) == -1) {
        perror("connect");
        return -1;
    }
    return 0;
}

/// Starts a client
/// @return 0 on success -1 on failure
int client() {
    char buf[BUFLEN];
    size_t linesize = BUFLEN;
    char *line = (char *) malloc(BUFLEN);
    while (getline(&line, &linesize, stdin) != -1) {
        if (strlen(line) <= 1) {
            break;
        }

        unsigned int num1;
        unsigned int num2;
        char op;

        memset(buf, 0, BUFLEN);

        int status = parse(line, strlen(line)+1, &num1, &num2, &op);

        if (status == -1) {
            report_error(buf, BUFLEN, "parse: failed to parse");
            continue;
        } else if (status == -2) {
            sprintf(buf, "%u%c%s", num1, '+', "0");
        } else {
            sprintf(buf, "%u%c%u", num1, op, num2);
        }

        if (send(SOCKFD, buf, strlen(buf)+1, 0) == -1) {
            perror("error on send");
            continue;
        }
        memset(buf, 0, BUFLEN);
        ssize_t re_bytes_c = recv(SOCKFD, buf, sizeof buf, 0);
        if (re_bytes_c == -1) {
            perror("error on receive");
            continue;
        }
        printf("%s", buf);
    }
    free(line);
    return 0;
}

/// The main method, starts depending on the args as client or server
/// @param argc argument count
/// @param argv array of options, -c for client followed by hostname and port
int main(int argc, char *argv[]) {
    SOCKFD = 0;
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = cleanup;
    sigaction(SIGTERM, &action, NULL);

    char* host = NULL;
    char* port = "5000";
    int afamiliy = AF_UNSPEC; /// should do v4 and v6 compatibility
    int flags = 0;

    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        CLIENT = 1; // client mode
        if (argc > 2) {
            host = argv[2];
            if (argc > 3) {
                port = argv[3];
            }
        }
    } else if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("usage: netcalc [-c hostname] [port]\n");
        return 0;
    } else {
        CLIENT = 0;
        afamiliy = AF_INET6;
        flags |= AI_PASSIVE;
        if (argc > 1) {
            port = argv[1];
        }
    }

    struct addrinfo *ainfo;
    if (prepaddr(host, port, &ainfo, afamiliy, flags) != 0) {
        fprintf(stderr, "prepaddr: failed to obtain adress.\n");
        return 1;
    }

    if ((SOCKFD = prepsocket(ainfo)) == -1) {
        return 1;
    }

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

    freeaddrinfo(ainfo); // free the linked list
    if (close(SOCKFD) != 0) {
        perror("close");
    }
    return 0;
}
