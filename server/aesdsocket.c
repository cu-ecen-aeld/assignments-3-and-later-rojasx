#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
// #include <unistd.h>
#include <stdio.h>
#include <string.h>

#define PORT "9000"
#define ERROR 0xFFFFFFFF
#define BACKLOGS 5
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

int writer(const char *writefile, const char *writestr)
{
    // Try to open, check for errors
    fd = open(writefile, (O_RDWR | O_CREAT), S_IRWXU);
    if (fd == -1)
    {
        // Check errno
        syslog(LOG_ERR, "Error: Failed to open file %s.", writefile);
        fprintf(stderr, "Error: Failed to open file %s.", writefile);
        return ERROR;
    }

    // If we opened, perform write, check for errors
    bytes_written = write(fd, writestr, len_string);
    if (bytes_written == -1)
    {
        /*error, check errno*/
        syslog(LOG_ERR, "Error: Failed to write to %s", writefile);
        fprintf(stderr, "Error: Failed to write to %s", writefile);
        return ERROR;
    }
    else if ((long unsigned int)bytes_written != len_string)
    {
        /*Manually output error*/
        syslog(LOG_ERR, "Error: Partial write! Bytes written != length of writestr.");
        return ERROR;
    }
    else
    {
        // Passing log
        syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
        printf("Writing %s to %s\n", writestr, writefile);
    }

    return 0;
}

// Ideas pulled from https://github.com/jasujm/apparatus-examples/blob/master/signal-handling/lib.c
void stream_shenanigans(int socket_fd)
{
    char recv_buf[1024];
    // char send_buf[1024];
    int send_bytes;
    char* buf_end;
    ssize_t len;
    int new_len
    FILE *output_file;

    // Read message into buffer
    memset(recv_buf, 0, sizeof(recv_buf));
    if ((len = recv(socket_fd, recv_buf, sizeof(recv_buf) - 1, MSG_WAITALL)) < 0)
    {
        fprintf(stderr, "ERROR calling recv\n");
        exit(ERROR);
    }
    
    // Determine end of packet
    buf_end = (char *)memchr(recv_buf, '\n', len);
    if (!buf_end)
    {
        fprintf(stderr, "ERROR, no newline detected\n");
        exit(ERROR);
    }
    new_len = (int)(buf_end - recv_buf);

    // Open file, write our message to it
    output_file = fopen(OUTPUT_FILE_PATH, "a+");
    if (!output_file)
    {
        fprintf(stderr, "ERROR opening output file for writing\n");
        exit(ERROR);
    }
    fprintf(output_file, "%.*s\n", new_len, recv_buf);
    fclose(output_file);

    // -------------------------------------------------------------
    // Send back contents of output file
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
    
    // Make a buffer with that length
    char send_buf[file_size];
    size_t result;
    int bytes_sent;
    
    // Read the contents into the buf
    result = fread((void *)send_buf, 1, file_size, output_file);
    if (result != file_size)
    {
        fprintf(stderr, "ERROR reading from file\n");
        fclose(output_file);
        exit(ERROR);
    }

    // Send it bro!!! |m/
    bytes_sent = send(socket_fd, (void *)send_buf, file_size, MSG_DONTWAIT);
    if (bytes_sent != file_size)
    {
        fprintf(stderr, "ERROR, didn't send all the bytes\n");
        exit(ERROR);
    }
    fclose(output_file);
}


int main(int argc, char *argv[])
{
    int sockfd;
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // points to results


    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Signal handling setup
    struct sigaction new_action;
    bool success = true;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;


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

    // Listen
    if (listen(sockfd, BACKLOGS) == ERROR)
    {
        fprintf(stderr, "ERROR listening\n");
        exit(ERROR);
    }

    // Accept
    openlog("AESD Socket Log", 0, LOG_USER);
    while(1)
    {
        int client_fd;
        struct sockaddr client_addr;
        socklen_t clientaddr_len = sizeof(sockaddr);
        client_fd = accept(sockfd, &client_addr, &clientaddr_len);
        if (client_fd == ERROR)
        {
            fprintf(stderr, "ERROR accepting new socket\n");
            exit(ERROR);
        }
        
        // Get the addr of the connection
        struct sockaddr_in *pV4addr = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = pV4addr->sin_addr;
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, addr_str, INET_ADDRSTRLEN);

        // Log the connection!!!
        syslog(LOG_INFO, "Accepted connection from %s\n", addr_str);

        // Do the recv, write, read, send
        stream_shenanigans();

        // Cleanup
        close(clientfd);
        syslog(LOG_INFO, "Closed connection from %s\n", addr_str);
    }

    close(sockfd);
    return 1;
}






// signal_handler()
// {
//     // Sets flags if SIGINT or SIGTERM is caught
// }

// main()
// {
//     // Registers signals and handler
//     // Does all the socket stuff
//     while (!SIGINT_flag || !SIGTERM_flag)
//     {
//         // Does socket connection and writes file, etc
//     }

//     // Closes sockets, files, and prints syslog to close gracefully
// }