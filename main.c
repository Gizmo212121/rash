#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

char** path = NULL;

int main(int argc, char* argv[])
{
    if (argc == 2)
    {
        printf("Not implemented yet!\n");
        return 0;
    }
    else if (argc > 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* line = NULL;
    size_t size = 0;
    ssize_t nread = 0;

    while (true)
    {
        printf("rash> ");

        if ((nread = getline(&line, &size, stdin)) != -1)
        {
            if (!strcmp("exit\n", line))
            {
                free(line);
                exit(0);
            }
            else if (!strcmp("ls\n", line))
            {
                pid_t pid = fork();
                // Some error code for SIGCHLD supposed to be here?

                switch(pid)
                {
                    case -1:
                        free(line);
                        perror("fork");
                        exit(EXIT_FAILURE);
                    case 0:
                    {
                        char** ls_args = NULL;
                        if (execv("/bin/ls", ls_args) == -1)
                        {
                            perror("execv");
                            exit(EXIT_FAILURE);
                        }
                    }
                    default:
                        wait(NULL);
                }
            }
        }
        else
        {
            printf("\n");
            free(line);
            exit(0);
        }
    }

    return 0;
}
