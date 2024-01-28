#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>


int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);

    // Check for args!
    if (argc != 3) {
        // syslog(LOG_ERR, "Invalid arguments.\n");
        syslog(LOG_ERR, "Error: Invalid Arguments.\nUsage: %s <writefile> <writestr>\n", argv[0]);
        fprintf(stderr, "Error: Invalid Arguments.\nUsage: %s <writefile> <writestr>\n", argv[0]);
        return 1;
    }


    int fd;
    const char *writefile = argv[1];
    const char *writestr = argv[2];
    size_t len_string = strlen(writestr);
    ssize_t bytes_written;

    if (strcmp(writestr, "")==0)
    {
        return 1;
    }

    // Try to open, check for errors
    fd = open(writefile, (O_RDWR | O_CREAT), S_IRWXU);
    if (fd == -1)
    {
        // Check errno
        syslog(LOG_ERR, "Error: Failed to open file %s.", writefile);
        fprintf(stderr, "Error: Failed to open file %s.", writefile);
        return 1;
    }

    // If we opened, perform write, check for errors
    bytes_written = write(fd, writestr, len_string);
    if (bytes_written == -1)
    {
        /*error, check errno*/
        syslog(LOG_ERR, "Error: Failed to write to %s", writefile);
        fprintf(stderr, "Error: Failed to write to %s", writefile);
        return 1;
    }
    else if ((long unsigned int)bytes_written != len_string)
    {
        /*Manually output error*/
        syslog(LOG_ERR, "Error: Partial write! Bytes written != length of writestr.");
        return 1;
    }
    else
    {
        // Passing log
        syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
        printf("Writing %s to %s\n", writestr, writefile);
    }

    closelog();
    return 0;
}


