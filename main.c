#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>

typedef struct s_vector
{
    char** data;
    size_t size;
    size_t capacity;
} s_vector;

typedef struct command
{
    size_t args_start;
    size_t args_end;
    char* stdin_redir;
    char* stdout_redir;
    char* stderr_redir;
    bool bg; // Foreground or background cmd
    struct command* pipe; // Piped command

} command;

void add_string(s_vector* lines, char* buffer, bool copy);
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread);
void erase(s_vector* vec, int pos);
void add_path(s_vector*, char* path_name);
void free_s_vector(s_vector* vector);
void execute_bin(const command* command, s_vector* tokens);
int num_args(const command* command);
int file_status(char* path_name);
int count_digits(int n);

void cd(const command* command, s_vector* tokens);
void prevd(const command* command);
void nextd(const command* command);
void dirh(const command* command);
void path(const command* command, s_vector* tokens);

void handle_command(const command* command, s_vector* args);
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

void print_command(const command* command, const s_vector* tokens)
{
    printf("\nCommand:\n\targs_start: %lu\n\targs_end: %lu\n", command->args_start, command->args_end);

    for (size_t j = command->args_start; j <= command->args_end; j++)
    {
        printf("\targ %lu: %s\n", j, tokens->data[j]);
    }

    if (command->stdin_redir) { printf("\n\tfn: %s\n", command->stdin_redir); }
    if (command->stdout_redir) { printf("\n\tfn: %s\n", command->stdout_redir); }
    if (command->stderr_redir) { printf("\n\tfn: %s\n", command->stderr_redir); }

    printf("\n");
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
    // if (buffer && !*buffer) { return; } // TODO: Could be bad

    if (lines->size == lines->capacity)
    {
        lines->capacity = (lines->capacity == 0) ? 1 : lines->capacity << 1;
        char** temp = realloc(lines->data, sizeof(*lines->data) * lines->capacity);
        if (!temp)
        {
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
                free(abs_path);
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
                    free(abs_path);
                }
                break;
            case 2: // Valid path but not dir
                printf("%s: not a directory\n", abs_path);
                free(abs_path);
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
int num_args(const command* command)
{
    return command->args_end - command->args_start + 1;
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
void execute_bin(const command* command, s_vector* tokens)
{
    bool contains_slashes = strchr(tokens->data[command->args_start], '/');
    char* path = NULL;

    if (contains_slashes)
    {
        path = realpath(tokens->data[command->args_start], NULL);
        if (!path)
        {
            if (errno == ENOENT)
            {
                printf("%s: no such file or directory\n", tokens->data[command->args_start]);
            }
            else
            {
                fprintf(stderr, "%s: ", tokens->data[command->args_start]);
                perror("realpath");
            }

            return;
        }

        int fs = file_status(path);

        if (fs == 1)
        {
            free(path);
            printf("%s: is a directory\n", tokens->data[command->args_start]);
            return;
        }

        if (access(path, X_OK))
        {
            fprintf(stderr, "%s: ", tokens->data[command->args_start]);
            perror("access");
            free(path);
            return;
        }
    }
    else
    {
        for (size_t i = 0; i < paths.size; i++)
        {
            if (asprintf(&path, "%s/%s", paths.data[i], tokens->data[command->args_start]) == -1)
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
            printf("%s: command not found\n", tokens->data[command->args_start]);
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
                // Change file descriptor if needed
                if (command->stdin_redir)
                {
                    if (!freopen(command->stdin_redir, "r", stdin))
                    {
                        perror("freopen");
                        exit(EXIT_FAILURE);
                    }
                }

                // Execute the command
                char* tmp = tokens->data[command->args_end + 1];
                tokens->data[command->args_end + 1] = NULL;

                if (execv(path, tokens->data + command->args_start) == -1)
                {
                    perror("execv");
                    exit(EXIT_FAILURE);
                }
                tokens->data[command->args_end] = tmp;
            }
            break;
        default:
            wait(NULL);
    }

    free(path);
}

// change directory built-in. If only 1 argument (i.e 'cd'), go to home
// If 2 arguments, change directory of process to relative or absolute path specified by 2nd arg
void cd(const command* command, s_vector* tokens)
{
    int num_cd_args = num_args(command);

    switch (num_cd_args)
    {
        case 1:
            // cd to home
            // TODO: NOT WORKING
            chdir(realpath("~", NULL));
            break;
        case 2:
        {
            char* clean_path = realpath(tokens->data[command->args_end], NULL);
            if (clean_path)
            {
                // printf("clean_path: %s\n", clean_path);

                int fs = file_status(clean_path);
                switch (fs)
                {
                    case 0:
                        // Does this ever happen?
                        printf("cd: %s: invalid path\n", tokens->data[command->args_end]);
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
                            // Overwrite dir history past at current dir if not on the current dir
                            if (dir_history.size != current_dir + 1)
                            {
                                erase(&dir_history, current_dir + 1);
                            }

                            add_string(&dir_history, clean_path, false);
                            current_dir++;
                        }
                        else
                        {
                            free(clean_path);
                        }

                        break;
                    case 2:
                        printf("cd: %s: not a directory\n", tokens->data[command->args_end]);
                        break;
                }
            }
            else
            {
                if (errno == ENOENT)
                {
                    printf("cd: %s: invalid path\n", tokens->data[command->args_end]);
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

// Takes parsed command and decides what to do with it
void handle_command(const command* command, s_vector* tokens)
{
    if (num_args(command) != 0)
    {
        char* first_arg = tokens->data[command->args_start];

        if      (!strcmp("exit"  , first_arg)) { exit(0); }
        else if (!strcmp("cd"    , first_arg)) { cd(command, tokens); }
        else if (!strcmp("prevd" , first_arg)) { prevd(command); }
        else if (!strcmp("nextd" , first_arg)) { nextd(command); }
        else if (!strcmp("dirh"  , first_arg)) { dirh(command); }
        else if (!strcmp("path"  , first_arg)) { path(command, tokens); }
        else                                   { execute_bin(command, tokens); }
    }
}

void print_prompt()
{
    // TODO: Read from config for prompt

    printf("\033[1;32m");
    printf("rash:");
    printf("\033[1;34m");
    printf("%s> ", dir_history.data[current_dir]);
    printf("\033[0m");
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

typedef enum DELIM
{
    ALPHANUMERIC,
    WHITESPACE,
    PIPE,
    OUT_REDIR,
    IN_REDIR,
    SEMI_COLON,
    AMPERSAND,
    QUOTE,
    DOUBLE_QUOTE,
} DELIM;

static const char* const delim_strings[] =
{
    "ALPHANUMERIC",
    "WHITESPACE",
    "PIPE",
    "OUT_REDIR",
    "IN_REDIR",
    "SEMI_COLON",
    "AMPERSAND",
    "QUOTE",
    "DOUBLE_QUOTE",
};


DELIM delimiter(char c)
{
    if (c == ' ' || c == '\t' || c == '\n') { return WHITESPACE; }
    else if (c == '|')  { return PIPE; }
    else if (c == '>')  { return OUT_REDIR; }
    else if (c == '<')  { return IN_REDIR; }
    else if (c == ';')  { return SEMI_COLON; }
    else if (c == '&')  { return AMPERSAND; }
    else if (c == '\'') { return QUOTE; }
    else if (c == '"')  { return DOUBLE_QUOTE; }
    else { return ALPHANUMERIC; }
}

bool tokenize(s_vector* tokens, char* buffer, ssize_t nread)
{
    if (nread == 1) { return false; }

    // Split input into discrete tokens in O(n) pass
    ssize_t token_start = 0;
    char replace = '\0';
    DELIM previous_delim = WHITESPACE;
    DELIM current_delim = WHITESPACE;
    bool need_closing_delim = false;

    for (int i = 0; i < nread; i++)
    {
        current_delim = delimiter(buffer[i]);

        if (current_delim == QUOTE || current_delim == DOUBLE_QUOTE)
        {
            if (!need_closing_delim)
            {
                previous_delim = current_delim;
                need_closing_delim = true;
                token_start = (ssize_t)i + 1;
            }
            else if (previous_delim == current_delim)
            {
                // Replace current position with null terminating
                buffer[i] = '\0';

                // Copy token to tokens
                if (i != token_start)
                    add_string(tokens, strndup(buffer + token_start, i - token_start), false);
                else // Empty string
                    add_string(tokens, "", true);

                // Add back original character
                buffer[i] = ' ';

                token_start = (ssize_t)i + 1;
                previous_delim = WHITESPACE;

                need_closing_delim = false;
            }
        }
        else if (!need_closing_delim)
        {
            if (current_delim != previous_delim)
            {
                if (previous_delim != WHITESPACE)
                {
                    // Replace current position with null terminating
                    replace = buffer[i];
                    buffer[i] = '\0';

                    // Copy token to tokens
                    char* token = strndup(buffer + token_start, i - token_start);
                    add_string(tokens, token, false);

                    // Add back original character
                    buffer[i] = replace;

                }

                token_start = (ssize_t)i;
                previous_delim = current_delim;
            }
            else
            {
                if (current_delim == WHITESPACE)
                {
                    token_start++;
                }
            }
        }
    }

    if (need_closing_delim)
    {
        fprintf(stderr, "missing closing delimiter: %s\n", delim_strings[previous_delim]);
        free_s_vector(tokens);
        return false;
    }

    if (!tokens->size) { return false; }
    else
    {
        // printf("Tokens size: %lu\nTokens cap: %lu\n", tokens->size, tokens->capacity);

        // for (size_t i = 0; i < tokens->size; i++)
        // {
        //     if (tokens->data[i])
        //         printf("%lu: %s\n", i, tokens->data[i]);
        // }

        return true;
    }
}

bool parse_tokens(s_vector* tokens)
{
    // Use calloc so struct members are 0-initialized
    command* commands = calloc(tokens->size, sizeof(*commands));

    bool done_taking_args = false;
    size_t current_command = 0;
    size_t arg_start = 0;

    for (size_t i = 0; i < tokens->size; i++)
    {
        int redir = 0;

        if ((!strcmp("<", tokens->data[i]) && (redir = 0))  ||
            (!strcmp(">", tokens->data[i]) && (redir = 1)) ||
            (!strcmp("2>", tokens->data[i]) && (redir = 2)))
        {
            if (i == 0)
            {
                fprintf(stderr, "syntax error near symbol %s: unexpected redirection\n", tokens->data[i]);
                goto syntax_error;
            }
            else
            {
                if (i + 1 < tokens->size)
                {
                    done_taking_args = true;

                    switch (redir)
                    {
                        case 0:
                            (commands + current_command)->stdin_redir = tokens->data[i++ + 1];
                            break;
                        case 1:
                            (commands + current_command)->stdout_redir = tokens->data[i++ + 1];
                            break;
                        case 2:
                            (commands + current_command)->stderr_redir = tokens->data[i++ + 1];
                            break;
                    }
                }
                else
                {
                    fprintf(stderr, "syntax error near %s: redirection missing file\n", tokens->data[i]);
                    goto syntax_error;
                }
            }
        }
        else if (!strcmp(";", tokens->data[i]))
        {
            done_taking_args = false;
            current_command++;
            arg_start = i + 1;
        }
        else
        {
            if (!done_taking_args)
            {
                (commands + current_command)->args_start = arg_start;
                (commands + current_command)->args_end = i;
            }
            else
            {
                fprintf(stderr, "%s: unexpected argument\n", tokens->data[i]);
                goto syntax_error;
            }
        }
    }

    add_string(tokens, NULL, false);

    for (size_t i = 0; i <= current_command; i++)
    {
        // print_command(&commands[i], tokens);
        handle_command(&commands[i], tokens);
    }

    free_s_vector(tokens);
    free(commands);

    return true;

    syntax_error:
        free_s_vector(tokens);
        free(commands);
        return false;
}

void run()
{
    char* buffer = NULL;
    size_t size = 0;
    ssize_t nread = 0;

    while (true)
    {
        s_vector tokens = {0};

        print_prompt();
        read_line(&buffer, &size, &nread);
        if (!tokenize(&tokens, buffer, nread)) { continue; }
        if (!parse_tokens(&tokens)) { continue; }
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

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    add_string(&dir_history, cwd, false);
}

// moves backward through the directory history
void prevd(const command* command)
{
    if (num_args(command) > 1)
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
void nextd(const command* command)
{
    if (num_args(command) > 1)
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
void dirh(const command* command)
{
    if (num_args(command) > 1)
    {
        fprintf(stderr, "prevd: too many arguments\n");
        return;
    }

    int max_digits = MAX(count_digits((int)dir_history.size - (int)current_dir), count_digits((int)current_dir));
    for (size_t i = 0; i < dir_history.size; i++)
    {
        int dist = abs((int)i - (int)current_dir);

        if (dist)
            printf("%*d) %s\n", max_digits, dist, dir_history.data[i]);
        else
        {
            printf("\033[1m"); // Bold for current dir
            printf("%*c  %s\n", max_digits, ' ', dir_history.data[i]);
            printf("\033[0m"); // Revert from bold
        }
    }
}

void path(const command* command, s_vector* tokens)
{
    int numargs = num_args(command);

    if (numargs == 1)
    {
        printf("paths:\n");
        for (size_t i = 0; i < paths.size; i++)
        {
            printf("\t%s\n", paths.data[i]);
        }
    }
    else
    {
        for (int i = 0; i < numargs - 1; i++)
        {
            add_path(&paths, tokens->data[command->args_start + 1 + i]);
        }
    }
}
