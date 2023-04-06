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
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>

#define PORT "9000"  // the port users will be connecting to
#define BACKLOG 40   // how many pending connections queue will hold
#define MAXDATASIZE 20000 // max number of bytes we can get at once 
#define AESD_SOCKET_PATH "/var/tmp/aesdsocketdata"

struct thread_info
{
    pthread_t t_id;
    int fd;
    char *ip;
    int fp;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct thread_info t_info[BACKLOG];
int t_count = 0;

int g_fp;
int g_sockfd;
int sig_term = 0;

void timestamp_handler(int signo)
{
    char outstr[100];
    time_t real_time;
    struct tm cur_time;

    if (time(&real_time) == -1) {
        syslog(LOG_ERR,"Unable to get real time clock value");
	    return;
    }

    localtime_r(&real_time, &cur_time);

    size_t strlen = strftime(outstr, 100, "timestamp:%a, %d %b %Y %T %z\n", &cur_time);
    if (!strlen) {
        syslog(LOG_ERR, "Unable to generate timestamp");
	    return;
    }

    pthread_mutex_lock(&mutex);

    if (write(g_fp, outstr, strlen) == -1) {
        syslog(LOG_ERR,"Failed to write timestamp");
        return;
    }

    pthread_mutex_unlock(&mutex);

    return;
}

void *threadfunc(void *args)
{
    char *recvbuf = (char *) malloc(MAXDATASIZE);
    size_t numbytes = 0;
    size_t recvbytes = 0;

    pthread_t t_id = ((struct thread_info *) args)->t_id;
    int fd = ((struct thread_info *) args)->fd;
    char *ip = ((struct thread_info *) args)->ip;
    int fp = ((struct thread_info *) args)->fp;
    
    while (1)
    {
        if ((recvbytes = recv(fd, recvbuf + recvbytes, MAXDATASIZE - numbytes, 0)) == -1) {
            perror("recv");
            free(recvbuf);
            exit(1);
        }

        numbytes += recvbytes;

        if (recvbuf[numbytes - 1] == 0x0A)
            break;
    }

    recvbuf[numbytes] = '\0';

    pthread_mutex_lock(&mutex);

    //write data to file
	write(fp, recvbuf, numbytes);

    //send data to client
    struct stat st;
    stat(AESD_SOCKET_PATH, &st);
    size_t writesize = st.st_size;

    char *sendbuf = (char*) malloc(writesize);

    lseek(fp, 0, SEEK_SET);
    read(fp, sendbuf, writesize);

    pthread_mutex_unlock(&mutex);

    if (send(fd, sendbuf, writesize, 0) == -1)
        perror("send");

    syslog(LOG_ERR, "Closed connection from %s", ip);

    free(sendbuf);
    free(recvbuf);
    close(fd);

    pthread_cancel(t_id);

    return args;
}

void signal_handler(int signo)
{
    if (signo == SIGINT)
		syslog(LOG_ERR, "Caught SIGINT, exiting");
	else if (signo == SIGTERM)
		syslog(LOG_ERR, "Caught SIGTERM, exiting");
	else {
		syslog(LOG_ERR, "Unexpected signal! %d", signo);
	}

    close(g_sockfd);
    sig_term = 1;
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
    int status;

	openlog(NULL, 0, LOG_USER);

    remove(AESD_SOCKET_PATH);

    pthread_mutex_init(&mutex, NULL);

	//To start a daemon process
	if ((argc > 1) && strcmp(argv[1],"-d") == 0)
	{
		if (daemon(0,0) == -1)
		{
			syslog(LOG_ERR, "Couldn't enter daemon mode");
			exit(1);
		}
	}

    g_fp = open(AESD_SOCKET_PATH, O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IROTH);
    if (g_fp == -1) {
        syslog(LOG_ERR, "Error opening file %s: %s", AESD_SOCKET_PATH, strerror(errno));
        exit(1);
    }

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
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

    if (signal (SIGINT, signal_handler) == SIG_ERR) {
        fprintf (stderr, "Cannot handle SIGINT!\n");
        exit (EXIT_FAILURE);
    }

    if (signal (SIGTERM, signal_handler) == SIG_ERR) {
        fprintf (stderr, "Cannot handle SIGTERM!\n");
        exit (EXIT_FAILURE);
    }

    if (signal (SIGALRM, timestamp_handler) == SIG_ERR) {
        fprintf (stderr, "Cannot handle SIGALRM!\n");
        exit (EXIT_FAILURE);
    }

    struct itimerval timer_val;
    timer_val.it_value.tv_sec  = 10;
    timer_val.it_value.tv_usec = 0;
    timer_val.it_interval.tv_sec  = 10;
    timer_val.it_interval.tv_usec = 0;

    tzset();

    if (setitimer(ITIMER_REAL, &timer_val, NULL) == -1) {
        syslog(LOG_ERR, "settimer failed");
        exit(1);
    }

    while (1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            syslog(LOG_ERR, "Bad socket");
            break;
        }

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);

		syslog(LOG_ERR, "Accepted connection from %s", s);

        struct thread_info *args = &t_info[t_count];
        args->fd = new_fd;
        args->ip = (char *) malloc(sizeof(s));
        memcpy(args->ip, s, sizeof(s));
        args->fp = g_fp;

        if (pthread_create(&t_info[t_count].t_id, NULL, threadfunc, &t_info[t_count])) {
            syslog(LOG_ERR, "pthread_create");
            exit(1);
        }

        for (int i=0; i< t_count; i++) {
            pthread_join(t_info[i].t_id, NULL);
        }

        t_count++;
    }

    for (int i=0; i< t_count; i++) {
        pthread_join(t_info[i].t_id, NULL);
        close(t_info[i].fd);
    }

    pthread_mutex_destroy(&mutex);
    close(g_sockfd);
    close(g_fp);
    
    return 0;
}