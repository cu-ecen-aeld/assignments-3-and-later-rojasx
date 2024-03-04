/*
    Written by: Xavier Rojas

*/
#include "queue.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT "9000"
#define ERROR 0xFFFFFFFF
#define BACKLOGS 5
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

#define RFC2822_FORMAT "timestamp:%a, %d %b %Y %T %z\n"
#define MAX_TIME_SIZE 60

volatile int sock_fd;
volatile int signal_caught = 0;
volatile int timer_caught = 0;

struct thread_data_s {
    pthread_t       thread_id;
    int             client_fd;
    pthread_mutex_t txn_mutex;
    bool            thread_complete;
    SLIST_ENTRY(thread_data_s) next_thread;
};

static void signal_handler(int sig)
{
    // Handle timer, then socket
    if (sig == SIGALRM)
    {
		timer_caught = 1;
	}
    else
    {
        // Just shutdown, cleanup happens in main
        shutdown(sock_fd, SHUT_RDWR);
        signal_caught = sig;
    }
}


// // Guided by https://www.thegeekstuff.com/2012/02/c-daemon-process/
static void start_daemon()
{
    closelog();
    openlog("AESD Socket Daemon Log", 0, LOG_USER);
    pid_t process_id = 0;
    pid_t sid = 0;

    // Child process
    process_id = fork();
    if (process_id < 0)
    {
        fprintf(stderr, "ERROR, could not fork\n");
        close(sock_fd);
        exit(ERROR);
    }

    // Kill parent
    if (process_id > 0)
    {
        syslog(LOG_INFO, "Killing parent process for daemon setup");
        exit(0);
    }

    // Set permissions
    umask(0);

    // New session
    sid = setsid();
    if (sid < 0)
    {
        fprintf(stderr, "ERROR, could not create new session\n");
        close(sock_fd);
        exit(ERROR);
    }

    // Change wd to root
    chdir("/");

    // Close pertinent files
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    // Redirect, got this from: https://groups.google.com/g/comp.unix.programmer/c/cJJoGqHmztI
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);

    syslog(LOG_INFO, "Daemon initialized");
}

void *time_stamp(void *data)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)data;
    // Setup 10 second timer
    FILE *output_file;
	struct itimerval my_timer;
	my_timer.it_value.tv_sec = 10;
	my_timer.it_value.tv_usec = 0;
	my_timer.it_interval.tv_sec = 10;
	my_timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &my_timer, NULL);
	char time_data[MAX_TIME_SIZE];
	memset(&time_data, 0, MAX_TIME_SIZE);
	time_t rawNow;
	struct tm* now = (struct tm*)malloc(sizeof(struct tm));
    // Start by writing time immediately
    timer_caught = 1;
    while(!signal_caught)
    {
        if(timer_caught)
        {
            timer_caught = 0;
			time(&rawNow);
			now = localtime_r(&rawNow, now);
			
			// Format timestamp
			memset(&time_data, 0, MAX_TIME_SIZE);
			strftime(time_data, MAX_TIME_SIZE, RFC2822_FORMAT, now);

			// Write timestamp to file
			pthread_mutex_lock(mutex);
            
            output_file = fopen(OUTPUT_FILE_PATH, "a+");
            if (!output_file)
            {
                fprintf(stderr, "ERROR opening output file for writing timestamp\n");
                exit(ERROR);
            }
            fwrite(time_data, strlen(time_data), 1, output_file);
            fclose(output_file);

			pthread_mutex_unlock(mutex);
        }
    }
    free(now);
    return data;
}


pthread_t start_time_thread(pthread_mutex_t *mutex)
{
    pthread_t time_thread_id;

    int time_thread_out = pthread_create(&time_thread_id, NULL, time_stamp, mutex);
    if (time_thread_out)
    {
        fprintf(stderr, "ERROR creating thread, pthread_create returned %d\n", time_thread_out);
        exit(ERROR);
    }
    else
    {
        syslog(LOG_INFO, "Starting timestamp thread %lu\n", time_thread_id);
    }

    return time_thread_id;
}


// Ideas pulled from https://github.com/jasujm/apparatus-examples/blob/master/signal-handling/lib.c
void *transaction(void *data)
{
    struct thread_data_s *thread_data = (struct thread_data_s *)data;
    FILE *output_file;
    int nbytes_buf = 512;
    int nbytes_recv;
    char* buf_end = NULL;
    char recv_buf[nbytes_buf];

    // Lock data
    pthread_mutex_lock(&(thread_data->txn_mutex));
    // Open file outside of the loop
    output_file = fopen(OUTPUT_FILE_PATH, "a+");
    if (!output_file)
    {
        fprintf(stderr, "ERROR opening output file for writing\n");
        exit(ERROR);
    }

    // Read message into buffer
    while (!buf_end)
    {
        // Always reset the buf no matter what
        memset(recv_buf, 0, sizeof(recv_buf));
        if ((nbytes_recv = recv(thread_data->client_fd, recv_buf, sizeof(recv_buf), 0)) < 0)
        {
            fprintf(stderr, "ERROR calling recv\n");
            exit(ERROR);
        }

        // Determine if end of packet
        buf_end = (char *)memchr(recv_buf, '\n', nbytes_recv);

        // If we never detected a newline, just append without a newline
        fwrite(recv_buf, nbytes_recv, 1, output_file);
    }
    fclose(output_file);
    // printf("DEBUG: wrote client buf to file\n");

    // -------------------------------------------------------------
    // Send back contents of output file
    output_file = fopen(OUTPUT_FILE_PATH, "r");
    if (!output_file)
    {
        fprintf(stderr, "ERROR opening output file for reading\n");
        exit(ERROR);
    }
    
    // Prep to read and send
    int nbytes_read;
    int nbytes_sent;
    char send_buf[nbytes_buf];
    
    // In case we send multiple packets per send, wait until eof
    while (!feof(output_file))
    {
        // Read the contents into the buf
        nbytes_read = fread(send_buf, 1, nbytes_buf, output_file);
        // Send it bro!!! |m/
        nbytes_sent = send(thread_data->client_fd, send_buf, nbytes_read, 0);
        if (nbytes_sent < 0)
        {
            fprintf(stderr, "ERROR, could not send\n");
            exit(ERROR);
        }
    }
    fclose(output_file);
    pthread_mutex_unlock(&(thread_data->txn_mutex));
    
    // Close client for next transaction
    close(thread_data->client_fd);
    thread_data->thread_complete = true;
    // printf("DEBUG: file contents sent!\n");
    // printf("%s", send_buf);
    return data;
}


int main(int argc, char *argv[])
{
    openlog("AESD Socket Log", 0, LOG_USER);

    // Determine if daemon
    bool daemon_flag = false;
    if ((argc > 2) || ((argc == 2) && (strcmp(argv[1], "-d"))))
    {
        fprintf(stderr, "ERROR, invalid arguments.\nUsage: aesdsocket\nOPTIONS\n\t[-d] Run as daemon.\n");
        exit(ERROR);
    }
    else if ((argc == 2) && (strcmp(argv[1], "-d") == 0))
    {
        daemon_flag = true;
        syslog(LOG_INFO, "Running aesdsocket as daemon.");
    }
    else
    {
        // No daemon flag, all is fine
        syslog(LOG_INFO, "Running aesdsocket as normal process.");
    }

    int status = 0;
    struct addrinfo hints;
    // struct addrinfo *servinfo = malloc(sizeof(struct addrinfo)); // Allocate memory
    struct addrinfo *servinfo; // Allocate memory
    memset(&hints, 0, sizeof(struct addrinfo));
    // memset(servinfo, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Signal handling setup
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &new_action, NULL))
    {
        fprintf(stderr, "ERROR, could not set up SIGTERM signal\n");
        exit(ERROR);
    }
    if (sigaction(SIGINT, &new_action, NULL))
    {
        fprintf(stderr, "ERROR, could not set up SIGINT\n");
        exit(ERROR);
    }
    if (sigaction(SIGALRM, &new_action, NULL))
    {
        fprintf(stderr, "ERROR, could not set up SIGALRM\n");
        exit(ERROR);
    }

    // Linked list setup
    SLIST_HEAD(slist_head, thread_data_s) head;
    SLIST_INIT(&head);

    // Make fd, use SO_REUSEADDR
    // https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    // int flags = fcntl(sock_fd, F_GETFL, 0);
    // fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    if (sock_fd == ERROR)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(ERROR);
    }
    int reuse = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == ERROR)
    {
        fprintf(stderr, "ERROR setting socket option SO_REUSEADDR\n");
        exit(ERROR);
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) == ERROR)
    {
        fprintf(stderr, "ERROR setting socket option SO_REUSEPORT\n");
        exit(ERROR);
    }
    printf("DEBUG: socket created\n");

    // Get the address info
    status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status)
    {
        fprintf(stderr, "ERROR getting addr info\n");
        exit(ERROR);
    }

    // Bind address to socket
    if (bind(sock_fd, servinfo->ai_addr, servinfo->ai_addrlen) == ERROR)
    {
        fprintf(stderr, "ERROR binding\n");
        exit(ERROR);
    }
    freeaddrinfo(servinfo);
    printf("DEBUG: socket bound\n");

    // Fork if daemon
    if (daemon_flag)
    {
        start_daemon();
    }

    // Start timer
    pthread_mutex_t my_mutex;           // Same mutex is used for socket data
    pthread_mutex_init(&my_mutex, NULL);
    pthread_t time_thread_id = start_time_thread(&my_mutex);

    // Listen
    if (listen(sock_fd, BACKLOGS) == ERROR)
    {
        fprintf(stderr, "ERROR listening\n");
        exit(ERROR);
    }
    // printf("DEBUG: listening\n");

    // Accept
    int client_fd;
    struct sockaddr client_addr;
    socklen_t clientaddr_len = sizeof(client_addr);
    char addr_str[INET_ADDRSTRLEN];

    while(!signal_caught)
    {
        pthread_t thread_id;
        client_fd = accept(sock_fd, &client_addr, &clientaddr_len);
        if ((client_fd != ERROR))
        {   
            // Get the addr of the connection
            struct sockaddr_in *pV4addr = (struct sockaddr_in *)&client_addr;
            struct in_addr ip_addr = pV4addr->sin_addr;
            inet_ntop(AF_INET, &ip_addr, addr_str, INET_ADDRSTRLEN);

            // Log the connection!!!
            syslog(LOG_INFO, "Accepted connection from %s\n", addr_str);

            // Start thread
            struct thread_data_s *my_thread_data = (struct thread_data_s *)malloc(sizeof(struct thread_data_s));
            if (!my_thread_data)
            {
                fprintf(stderr, "ERROR allocating space for thread\n");
                exit(ERROR);
            }
            my_thread_data->thread_complete = false;
            my_thread_data->client_fd = client_fd;
            my_thread_data->txn_mutex = my_mutex;
            int thread_out = pthread_create(&thread_id, NULL, transaction, my_thread_data);
            if (thread_out)
            {
                free(my_thread_data);
                fprintf(stderr, "ERROR creating thread, cleaning up and closing");
                break;
            }
            my_thread_data->thread_id = thread_id;
            syslog(LOG_INFO, "Starting new thread %lu\n", my_thread_data->thread_id);

            // Add this thread to LL
            SLIST_INSERT_HEAD(&head, my_thread_data, next_thread);

            // Remove completed threads
            struct thread_data_s *tmp_thread_data;
            struct thread_data_s *tmp_thread_next;
            SLIST_FOREACH_SAFE(tmp_thread_data, &head, next_thread, tmp_thread_next)
            {
                if (tmp_thread_data->thread_complete)
                {
                    syslog(LOG_INFO, "Thread %lu complete, joining.\n", tmp_thread_data->thread_id);
                    pthread_join(tmp_thread_data->thread_id, NULL);
                    SLIST_REMOVE(&head, tmp_thread_data, thread_data_s, next_thread);
                    free(tmp_thread_data);
                }
            }
            syslog(LOG_INFO, "Closed connection from %s\n", addr_str);
        }
    }

    // Cleanup
    // First check for remaining links
    while(!SLIST_EMPTY(&head))
    {
        struct thread_data_s *elem = SLIST_FIRST(&head);
        // Join here as well?
        // pthread_join(elem->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, next_thread);
        free(elem);
    }
    pthread_join(time_thread_id, NULL);
    pthread_mutex_destroy(&my_mutex);
    close(client_fd);
    close(sock_fd);
    remove(OUTPUT_FILE_PATH);
    syslog(LOG_USER, "Caught signal: %s. Exiting!", strsignal(signal_caught));
    closelog();
    printf("\nreturning successfully\n\n");
    return 0;
}