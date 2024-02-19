/*
    Written by: Xavier Rojas

*/
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define PORT "9000"
#define ERROR 0xFFFFFFFF
#define BACKLOGS 5
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

bool SIGINT_flag = false;
bool SIGTERM_flag = false;
int sockfd;
int client_fd;

static void signal_handler(int sig)
{
    // Cleanup time
    shutdown(sockfd, SHUT_RDWR);
    close(client_fd);
    close(sockfd);
    remove(OUTPUT_FILE_PATH);
    syslog(LOG_USER, "Caught signal %d, exiting", sig);
    closelog();
}


// Ideas pulled from https://github.com/jasujm/apparatus-examples/blob/master/signal-handling/lib.c
void transaction(int client_fd)
{
    char recv_buf[500];
    // char send_buf[1024];
    int send_bytes;
    char* buf_end = NULL;
    ssize_t len;
    int new_len;
    FILE *output_file;

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
        memset(recv_buf, 0, sizeof(recv_buf));
        if ((len = recv(client_fd, recv_buf, sizeof(recv_buf), 0)) < 0)
        {
            fprintf(stderr, "ERROR calling recv\n");
            exit(ERROR);
        }
        printf("DEBUG: received client buffer\n");
        
        // Determine end of packet
        buf_end = (char *)memchr(recv_buf, '\n', len);

        // If we never detected a newline, just append without a newline
        if (!buf_end)
        {
            fprintf(output_file, "%s", recv_buf);
            // printf("size of this buf is %d\n", (int)len);
        }
        else
        {
            len = (int)(buf_end - recv_buf);
            fprintf(output_file, "%.*s\n", (int)len, recv_buf);
            // printf("size of this buf FINAL is %d\n", (int)len);
        }
    }
    fclose(output_file);
    printf("DEBUG: wrote client buf to file\n");

    // -------------------------------------------------------------
    // Send back contents of output file
    int file_size;
    output_file = fopen(OUTPUT_FILE_PATH, "r");
    if (!output_file)
    {
        fprintf(stderr, "ERROR opening output file for reading\n");
        exit(ERROR);
    }
    // Get the file size
    fseek(output_file, 0, SEEK_END);
    file_size = ftell(output_file);
    rewind(output_file);
    printf("DEBUG: opened output file\n");
    
    // Make a buffer with that length
    char send_buf[file_size];
    size_t result;
    int bytes_sent;
    
    // Read the contents into the buf
    result = fread(send_buf, 1, file_size, output_file);
    if (result != file_size)
    {
        fprintf(stderr, "ERROR reading from file\n");
        fclose(output_file);
        exit(ERROR);
    }
    printf("DEBUG: read file contents to buf\n");

    // Send it bro!!! |m/
    bytes_sent = send(client_fd, send_buf, result, 0);
    if (bytes_sent != file_size)
    {
        fprintf(stderr, "ERROR, didn't send all the bytes\n");
        exit(ERROR);
    }
    fclose(output_file);
    printf("DEBUG: file contents sent!\n");
    printf("%s", send_buf);
}


int main(int argc, char *argv[])
{
    openlog("AESD Socket Log", 0, LOG_USER);

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
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == ERROR)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(ERROR);
    }
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == ERROR)
    {
        fprintf(stderr, "ERROR setting socket option SO_REUSEADDR\n");
        exit(ERROR);
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) == ERROR)
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
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == ERROR)
    {
        fprintf(stderr, "ERROR binding\n");
        exit(ERROR);
    }
    freeaddrinfo(servinfo);
    printf("DEBUG: socket bound\n");

    // Listen
    if (listen(sockfd, BACKLOGS) == ERROR)
    {
        fprintf(stderr, "ERROR listening\n");
        exit(ERROR);
    }
    printf("DEBUG: listening\n");

    // Accept
    struct sockaddr client_addr;
    socklen_t clientaddr_len = sizeof(client_addr);
    char addr_str[INET_ADDRSTRLEN];
    while(1)
    {
        client_fd = accept(sockfd, &client_addr, &clientaddr_len);
        if (client_fd == ERROR)
        {
            fprintf(stderr, "ERROR accepting new socket\n");
            exit(ERROR);
        }
        printf("DEBUG: client accepted\n");
        
        // Get the addr of the connection
        struct sockaddr_in *pV4addr = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = pV4addr->sin_addr;
        inet_ntop(AF_INET, &ip_addr, addr_str, INET_ADDRSTRLEN);

        // Log the connection!!!
        syslog(LOG_INFO, "Accepted connection from %s\n", addr_str);

        // Do the recv, write, read, send
        transaction(client_fd);

        // Cleanup
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s\n", addr_str);
    }
    
    return 0;
}