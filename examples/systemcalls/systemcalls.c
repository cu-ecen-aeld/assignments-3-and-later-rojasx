#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret;
    ret = system(cmd);
    if (ret == -1)
    {
        return false;
    }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // Xavier's code
    // ------------------------
    pid_t pid;
    int status;

    // Fork and check
    pid = fork();
    if (pid == -1)
    {
        perror("Fork failed");
        return false;
    }
    else if (pid == 0)
    {
        // Execute command
        if (execv(command[0], command) == -1)
        {
            perror("Execv failed");
            exit(EXIT_FAILURE);
        }
    }

    // Check on wait and if good check on exit status of child process
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("wait failed");
        // printf("\nStatus of child process was %d\n", status);
        return false;
    }
    else if ((WEXITSTATUS(status) != 0) || WIFSIGNALED(status))
    {
        // printf("\nxStatus of child process wasx %d\n", WEXITSTATUS(status));
        // printf("command was %s \n", command[0]);
        printf("Exit status of child process non-zero or exited with WIFSIGNALED.\n");
        return false;
    }
    // ------------------------
    // Xavier's code

    va_end(args);
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // ------------------------
    // Xavier's code
    pid_t pid;
    int status;
    int fd;

    // Open and check file
    fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0)
    {
        perror("Failed to open outputfile");
        return false;
    }

    // Fork and check
    pid = fork();
    if (pid == -1)
    {
        perror("Fork failed");
        return false;
    }
    else if (pid == 0)
    {
        // dup2 to redirect stdout!
        dup2(fd, 1);
        if ( execv(command[0], command) == -1)
        {
            perror("Execv failed");
            exit(EXIT_FAILURE);
        }
    }

    // Check on wait and if good check on exit status of child process
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("wait failed");
        // printf("\nStatus of child process was %d\n", status);
        return false;
    }
    else if ((WEXITSTATUS(status) != 0) || WIFSIGNALED(status))
    {
        // printf("\nxStatus of child process wasx %d\n", WEXITSTATUS(status));
        // printf("command was %s \n", command[0]);
        printf("Exit status of child process non-zero or exited with WIFSIGNALED.\n");
        return false;
    }
    close(fd);
    // ------------------------
    // Xavier's code

    va_end(args);

    return true;
}
