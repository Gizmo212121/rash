#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

typedef struct s_vector
{
    char** data;
    size_t size;
    size_t capacity;
} s_vector;

void add_string(s_vector* lines, char* buffer, bool copy);
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread);
void erase(s_vector* vec, int pos);
void add_path(s_vector*, char* path_name);
void free_s_vector(s_vector* vector);
void execute_bin(char** args);
int num_args(char** args);
int file_status(char* path_name);
int count_digits(int n);

void cd(s_vector* args);
void prevd(s_vector* args);
void nextd(s_vector* args);
void dirh(s_vector* args);

void parse_input(s_vector* args, char* input_buffer);
void handle_command_args(s_vector* args);
void print_prompt();
void read_line(char** buffer, size_t* size, ssize_t* nread);
void init(int argc, char* argv[]);
void run();


s_vector paths = {NULL, 0, 0};
s_vector lines = {NULL, 0, 0};
s_vector dir_history = {NULL, 0, 0};
size_t current_dir = 0;

void clean_up_mem()
{
    free_s_vector(&paths);
    free_s_vector(&lines);
    free_s_vector(&dir_history);
}


int main(int argc, char* argv[])
{
    init(argc, argv);
    run();

    return EXIT_SUCCESS;
}

// Add string to dynamic array of strings
// Capacity doubles when full and attempting to add string
// If copy is true, the original buffer is copied. If copy is false, the pointer is copied
void add_string(s_vector* lines, char* buffer, bool copy)
{
    if (buffer && !*buffer) { return; }

    if (lines->size == lines->capacity)
    {
        lines->capacity = (lines->capacity == 0) ? 1 : lines->capacity << 1;
        char** temp = realloc(lines->data, sizeof(*lines->data) * lines->capacity);
        if (!temp)
        {
            // TODO: Free on exit?
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        lines->data = temp;
    }

    if (copy)
        lines->data[lines->size] = buffer ? strdup(buffer) : NULL;
    else
        lines->data[lines->size] = buffer;

    lines->size++;
}

// add_string but works on input buffer of variable length where actual string size might not match
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread)
{
    if (buffer && !*buffer) { return; }

    if (lines->size == lines->capacity)
    {
        lines->capacity = (lines->capacity == 0) ? 1 : lines->capacity << 1;
        char** temp = realloc(lines->data, sizeof(*lines->data) * lines->capacity);
        if (!temp)
        {
            // TODO: Free on exit?
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        lines->data = temp;
    }

    lines->data[lines->size] = buffer ? strndup(buffer, nread) : NULL;
    lines->size++;
}

void erase(s_vector* vec, int pos)
{
    if (pos < 0 || pos >= (int)vec->size) { fprintf(stderr, "erase: invalid range\n"); exit(EXIT_FAILURE); }

    for (int i = pos; i < (int)vec->size; i++)
    {
        free(vec->data[i]);
        vec->data[i] = NULL;
    }

    vec->size = pos;
}

// Adds path path_name to PATH. Cleans path_name for absolute syntax, resolving symlinks, checking if the path_name is valid, and checking if the path_name already exists in PATH.
void add_path(s_vector* paths, char* path_name)
{
    char* abs_path = realpath(path_name, NULL);
    if (abs_path)
    {
        int fs = file_status(abs_path);

        switch (fs)
        {
            case 0:
                printf("%s: invalid path\n", abs_path);
                break;
            case 1: // Valid path and dir
                // check if the new path is already in the path

                bool already_has_path = false;
                for (size_t i = 0; i < paths->size; i++)
                {
                    if (!strcmp(abs_path, paths->data[i]))
                    {
                        already_has_path = true;
                        break;
                    }
                }

                if (!already_has_path)
                {
                    add_string(paths, abs_path, false);
                }
                else
                {
                    printf("%s: already in path\n", abs_path);
                }
                break;
            case 2: // Valid path but not dir
                printf("%s: not a directory\n", abs_path);
                break;
        }
    }
    else
    {
        printf("%s: invalid path\n", path_name);
    }
}

// Determine number of arguments by finding null argument
// Initial command is the 1'st argument, so 'cd' has 1 arguments, 'cd ..' has 2 argument, and args with just the null argument is 0 arguments
// Args should always be at least size 1
int num_args(char** args)
{
    int i = 0;
    while (true)
    {
        if (!args[i]) { break; }
        i++;
    }

    return i;
}

// Takes an absolute path to a file and determines if it's a directory, a file, nonexistent, or invalid
// Returns 0 if the file doesn't exist
// Returns 1 if the file exists and it's a directory
// Returns 2 if the file exists and it's not a directory
int file_status(char* path_name)
{
    struct stat s;
    int err = stat(path_name, &s);
    if(-1 == err)
    {
        if(ENOENT == errno)
        {
            /* does not exist */
            return 0;
        }
        else
        {
            perror("stat");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if(S_ISDIR(s.st_mode))
        {
            /* it's a dir */
            return 1;
        }
        else
        {
            /* exists but is no dir */
            return 2;
        }
    }
}

// free's memory of s_vector
void free_s_vector(s_vector* vector)
{
    for (size_t i = 0; i < vector->size; i++)
    {
        free(vector->data[i]);
    }
    free(vector->data);
}

// Runs an executable given args. If original command contains slashes, it's assumed to be relative or absolute path executable. Otherwise, the command is assumed to be some executable found in PATH
void execute_bin(char** args)
{
    bool contains_slashes = strchr(args[0], '/');
    char* path = NULL;

    if (contains_slashes)
    {
        path = realpath(args[0], NULL);
        if (!path)
        {
            if (errno == ENOENT)
            {
                printf("%s: no such file or directory\n", args[0]);
            }
            else
            {
                fprintf(stderr, "%s: ", args[0]);
                perror("realpath");
            }

            return;
        }

        int fs = file_status(path);

        if (fs == 1)
        {
            free(path);
            printf("%s: is a directory\n", args[0]);
            return;
        }

        if (access(path, X_OK))
        {
            fprintf(stderr, "%s: ", args[0]);
            perror("access");
            free(path);
            return;
        }
    }
    else
    {
        for (size_t i = 0; i < paths.size; i++)
        {
            if (asprintf(&path, "%s/%s", paths.data[i], args[0]) == -1)
            {
                perror("asprintf");
                exit(EXIT_FAILURE);
            }

            if (!access(path, X_OK))
            {
                break;
            }

            free(path);
            path = NULL;
        }

        if (!path)
        {
            printf("%s: command not found\n", args[0]);
            return;
        }

    }

    // Execute command
    pid_t pid = fork();
    // Some error code for SIGCHLD supposed to be here?

    switch(pid)
    {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
        case 0:
            {
                if (execv(path, args) == -1)
                {
                    perror("execv");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        default:
            wait(NULL);
    }

    free(path);
}

// change directory built-in. If only 1 argument (i.e 'cd'), go to home
// If 2 arguments, change directory of process to relative or absolute path specified by 2nd arg
void cd(s_vector* args)
{
    int num_cd_args = num_args(args->data);

    switch (num_cd_args)
    {
        case 1:
            // cd to home
            // TODO: NOT WORKING
            chdir(realpath("~", NULL));
            break;
        case 2:
        {
            char* clean_path = realpath(args->data[1], NULL);
            if (clean_path)
            {
                int fs = file_status(clean_path);
                switch (fs)
                {
                    case 0:
                        // Does this ever happen?
                        printf("cd: %s: invalid path\n", args->data[1]);
                        break;
                    case 1:
                        if (chdir(clean_path) == -1)
                        {
                            perror("chdir");
                            exit(EXIT_FAILURE);
                        }

                        // Add dir path to dir_history
                        if (strcmp(dir_history.data[current_dir], clean_path))
                        {
                            if (dir_history.size == current_dir + 1)
                            {
                                add_string(&dir_history, clean_path, false);
                                current_dir++;
                            }
                            else
                            {
                                erase(&dir_history, current_dir + 1);
                                add_string(&dir_history, clean_path, false);
                                current_dir++;
                            }
                        }
                        else
                        {
                            free(clean_path);
                        }

                        break;
                    case 2:
                        printf("cd: %s: not a directory\n", args->data[1]);
                        break;
                }
            }
            else
            {
                if (errno == ENOENT)
                {
                    printf("cd: %s: invalid path\n", args->data[1]);
                }
                else
                {
                    perror("realpath\n");
                    exit(EXIT_FAILURE);
                }
            }

            break;
        }
        default:
            printf("cd: too many arguments\n");
            break;
    }
}

// Parses input into constituent commands
void parse_input(s_vector* args, char* input_buffer)
{
    char* token = NULL;
    while ((token = strsep(&input_buffer, " \n\t")))
    {
        add_string(args, token, true);
    }
    add_string(args, NULL, true);
}

// Takes parsed commands and decides what to do with them
void handle_command_args(s_vector* args)
{
    if (num_args(args->data) == 0)
    {
        free_s_vector(args);
        return;
    }

    if      (!strcmp("exit", args->data[0]))  { exit(0); }
    else if (!strcmp("cd", args->data[0]))    { cd(args); }
    else if (!strcmp("prevd", args->data[0])) { prevd(args); }
    else if (!strcmp("nextd", args->data[0])) { nextd(args); }
    else if (!strcmp("dirh", args->data[0]))  { dirh(args); }
    else                                      { execute_bin(&args->data[0]); }

    free_s_vector(args);
}

void print_prompt()
{
    // TODO: Read from config for prompt
    printf("rash:%s> ", dir_history.data[current_dir]);
}

void read_line(char** buffer, size_t* size, ssize_t* nread)
{
    if ((*nread = getline(buffer, size, stdin)) != -1)
    {
        // If read line isn't just a newline, add it to the lines s_vector
        if (**buffer != '\n')
        {
            buf_add_string(&lines, *buffer, *nread);
        }
    }
    else
    {
        perror("getline");
        exit(EXIT_FAILURE);
    }
}

void run()
{
    char* buffer = NULL;
    size_t size = 0;
    ssize_t nread = 0;

    while (true)
    {
        s_vector args = {NULL, 0, 0};

        print_prompt();
        read_line(&buffer, &size, &nread);
        parse_input(&args, buffer);
        handle_command_args(&args);
    }

    clean_up_mem();
}

void init(int argc, char* argv[])
{
    if (argc == 2)
    {
        printf("Not implemented yet!\n");
        exit(EXIT_SUCCESS);
    }
    else if (argc > 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    add_path(&paths, "/bin/");
    add_path(&paths, "/usr/local/bin/");
    add_path(&paths, "/home/neogizmo/gizmo/programming_projects/razz/build");

    add_string(&dir_history, get_current_dir_name(), false);
}

// moves backward through the directory history
void prevd(s_vector* args)
{
    if (num_args(args->data) > 1)
    {
        printf("prevd: too many arguments\n");
        return;
    }

    if (current_dir == 0)
    {
        printf("prevd: already at earliest directory\n");
        return;
    }

    if (chdir(dir_history.data[--current_dir]) == -1)
    {
        perror("chdir");
        exit(EXIT_FAILURE);
    }
}

// moves forward through the directory history
void nextd(s_vector* args)
{
    if (num_args(args->data) > 1)
    {
        printf("prevd: too many arguments\n");
        return;
    }

    if (current_dir == dir_history.size - 1)
    {
        printf("prevd: already at current directory\n");
        return;
    }

    if (chdir(dir_history.data[++current_dir]) == -1)
    {
        perror("chdir");
        exit(EXIT_FAILURE);
    }
}

int count_digits(int n)
{
    int count = 0;
    if (n == 0)
    {
        return 1;
    }
    if (n < 0)
    {
        n = -n;
    }
    while (n != 0)
    {
        n /= 10;
        count++;
    }
    return count;
}

// prints directory history
void dirh(s_vector* args)
{
    if (num_args(args->data) > 1)
    {
        printf("prevd: too many arguments\n");
        return;
    }

    int max_digits = MAX(count_digits((int)dir_history.size - (int)current_dir), count_digits((int)current_dir));
    for (size_t i = 0; i < dir_history.size; i++)
    {
        int dist = abs((int)i - (int)current_dir);

        if (dist)
            printf("%*d) %s\n", max_digits, dist, dir_history.data[i]);
        else
            printf("%*c  %s\n", max_digits, ' ', dir_history.data[i]);
    }
}
