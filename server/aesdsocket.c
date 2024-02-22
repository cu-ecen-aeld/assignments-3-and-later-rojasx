/*
    Written by: Xavier Rojas

*/
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

#define PORT "9000"
#define ERROR 0xFFFFFFFF
#define BACKLOGS 5
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

volatile int sock_fd;
volatile int signal_caught = 0;

static void signal_handler(int sig)
{
    // Just shutdown, cleanup happens in main
    shutdown(sock_fd, SHUT_RDWR);
    signal_caught = sig;
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

// Ideas pulled from https://github.com/jasujm/apparatus-examples/blob/master/signal-handling/lib.c
void transaction(int client_fd)
{
    FILE *output_file;
    int nbytes_buf = 512;
    int nbytes_recv;
    char* buf_end = NULL;
    char recv_buf[nbytes_buf];

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
        // Alwas reset the buf no matter what
        memset(recv_buf, 0, sizeof(recv_buf));
        if ((nbytes_recv = recv(client_fd, recv_buf, sizeof(recv_buf), 0)) < 0)
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
        nbytes_sent = send(client_fd, send_buf, nbytes_read, 0);
        if (nbytes_sent < 0)
        {
            fprintf(stderr, "ERROR, could not send\n");
            exit(ERROR);
        }
    }
    fclose(output_file);
    // printf("DEBUG: file contents sent!\n");
    // printf("%s", send_buf);
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
    struct addrinfo *servinfo = malloc(sizeof(struct addrinfo)); // Allocate memory
    memset(&hints, 0, sizeof(struct addrinfo));
    memset(servinfo, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;
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

    // Make fd, use SO_REUSEADDR
    // https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
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
        client_fd = accept(sock_fd, &client_addr, &clientaddr_len);
        if ((client_fd == ERROR) && signal_caught)
        {
            // We will only be able to catch the signal if caught waiting in accept
            // If signal thrown below, unsure of what will happen...
            break;
        }
        else if ((client_fd == ERROR) && !signal_caught)
        {
            fprintf(stderr, "ERROR accepting new socket\n");
            exit(ERROR);
        }
        // printf("DEBUG: client accepted\n");
        
        // Get the addr of the connection
        struct sockaddr_in *pV4addr = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = pV4addr->sin_addr;
        inet_ntop(AF_INET, &ip_addr, addr_str, INET_ADDRSTRLEN);

        // Log the connection!!!
        syslog(LOG_INFO, "Accepted connection from %s\n", addr_str);

        // Do the recv, write, read, send
        transaction(client_fd);

        // Close client for next transaction
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s\n", addr_str);
    }

    // Cleanup
    close(client_fd);
    close(sock_fd);
    remove(OUTPUT_FILE_PATH);
    syslog(LOG_USER, "Caught signal: %s. Exiting!", strsignal(signal_caught));
    closelog();
    
    return 0;
}