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

typedef struct s_vector
{
    char** data;
    size_t size;
    size_t capacity;
} s_vector;

void add_string(s_vector* lines, char* buffer);
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread);
void add_path(s_vector*, char* path_name);
void free_s_vector(s_vector* vector);
void execute_bin(char** args);
int num_args(char** args);
int file_status(char* path_name);
void update_cwd();

void cd(s_vector* args);

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

char current_working_directory[PATH_MAX];


int main(int argc, char* argv[])
{
    init(argc, argv);
    run();

    return EXIT_SUCCESS;
}

// Add string to dynamic array of strings
// Capacity doubles when full and attempting to add string
void add_string(s_vector* lines, char* buffer)
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

    lines->data[lines->size] = buffer ? strdup(buffer) : NULL;
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
                    add_string(paths, abs_path);
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

    free(abs_path);
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

void update_cwd()
{
    if (getcwd(current_working_directory, sizeof(current_working_directory)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
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

                        update_cwd();

                        // Add dir path to dir_history
                        // TODO:
                        if (dir_history.data[current_dir] && !strcmp(dir_history.data[current_dir], clean_path))
                        {
                            add_string(&dir_history, clean_path);
                            current_dir++;
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

            free(clean_path);

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
        add_string(args, token);
    }
    add_string(args, NULL);
}

// Takes parsed commands and decides what to do with them
void handle_command_args(s_vector* args)
{
    if (num_args(args->data) == 0)
    {
        free_s_vector(args);
        return;
    }

    if (!strcmp("exit", args->data[0]))
    {
        exit(0);
    }
    else if (!strcmp("cd", args->data[0]))
    {
        cd(args);
    }
    else
    {
        execute_bin(&args->data[0]);
    }

    free_s_vector(args);
}

void print_prompt()
{
    // TODO: Read from config for prompt
    printf("rash:%s> ", current_working_directory);
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

    update_cwd();

    // Add cwd to directory history
    int len = strlen(current_working_directory);
    char* cwd = strndup(current_working_directory, len);
    add_string(&dir_history, cwd);
    free(cwd);
}
