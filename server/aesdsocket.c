#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>

#define PORT "9000"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold
#define MAXDATASIZE 20000 // max number of bytes we can get at once 
#define AESD_SOCKET_PATH "/var/tmp/aesdsocketdata"

int g_sockfd, g_new_fd;
int isTerminalted = 0;
char buf[MAXDATASIZE];

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void signal_handler(int signo)
{
    if (signo == SIGINT)
		syslog(LOG_ERR, "Caught signal, exiting");
	else if (signo == SIGTERM)
		syslog(LOG_ERR, "Caught signal, exiting");
	else {
		syslog(LOG_ERR, "Unexpected signal!");
	}

    close(g_new_fd);
    close(g_sockfd);
    isTerminalted = 1;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
	size_t numbytes = 0;

	openlog(NULL, 0, LOG_USER);
    remove(AESD_SOCKET_PATH);

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            exit(-1);
        }
        
        g_sockfd = sockfd;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

	if (signal (SIGCHLD, sigchld_handler) == SIG_ERR) {
		fprintf (stderr, "Cannot handle SIGINT!\n");
		exit (EXIT_FAILURE);
	}

	if (signal (SIGINT, signal_handler) == SIG_ERR) {
		fprintf (stderr, "Cannot handle SIGINT!\n");
		exit (EXIT_FAILURE);
	}

	if (signal (SIGTERM, signal_handler) == SIG_ERR) {
		fprintf (stderr, "Cannot handle SIGTERM!\n");
		exit (EXIT_FAILURE);
	}

	//To start a daemon process
	if ((argc > 1) && strcmp(argv[1],"-d") == 0)
	{
		if (daemon(0,0) == -1)
		{
			syslog(LOG_ERR, "Couldn't enter daemon mode");
			exit(1);
		}
	}

    isTerminalted = 0;
    size_t size = 0;

    while (!isTerminalted) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            syslog(LOG_ERR, "Bad socket");
            continue;
        }

        g_new_fd = new_fd;

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);

		syslog(LOG_ERR, "Accepted connection from %s", s);

		if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
			perror("recv");
			exit(1);
		}
        buf[numbytes] = '\0';

		//write data to file
		FILE *fp = fopen (AESD_SOCKET_PATH, "a");
		if (fp == NULL) {
			syslog(LOG_ERR, "Error opening file %s: %s", AESD_SOCKET_PATH, strerror(errno));
			exit(1);
		}
		fprintf (fp, "%s", buf);
		fclose (fp);

		//send data to client
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            struct stat st;
            stat(AESD_SOCKET_PATH, &st);
            size = st.st_size;
            
            char *sendbuf = (char*) malloc(size);
            FILE *fp1 = fopen (AESD_SOCKET_PATH, "r");
            if (fp1 == NULL) {
                syslog(LOG_ERR, "Error opening file %s: %s", AESD_SOCKET_PATH, strerror(errno));
                exit(1);
            }
            fread(sendbuf, 1, size, fp1);
            fclose(fp1);

            if (send(new_fd, sendbuf, size, 0) == -1)
                perror("send");

			free(sendbuf);
            close(new_fd);

			syslog(LOG_ERR, "Closed connection from %s", s);
            exit(0);
        }

        close(new_fd);  // parent doesn't need this
    }

    close(g_new_fd);
    close(g_sockfd);

    return 0;
}