#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>

line interactive_line = {0};
s_vector paths = {NULL, 0, 0};
s_vector dir_history = {NULL, 0, 0};
size_t current_dir = 0;

s_vector line_history = {NULL, 0, 0};
ssize_t line_history_search_index = 0;
line temp_line = {0};
bool search_initiated = false;

size_t prompt_start_y = 0;
size_t prompt_end_y = 0;
size_t prompt_length = 0;

void clean_up_mem()
{
    free_s_vector(&paths);
    free_s_vector(&line_history);
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

                if (command->stdout_redir)
                {
                    if (!freopen(command->stdout_redir, "w", stdout))
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
    printf("\033[1;32m");
    printf("rash:");
    printf("\033[1;34m");
    printf("%s> ", dir_history.data[current_dir]);
    printf("\033[0m");
}

void refresh_prompt(bool flush)
{
    cursor_pos pos = get_cursor();
    prompt_start_y = pos.y;

    print_prompt();

    if (flush)
        fflush(stdout);

    pos = get_cursor();
    prompt_end_y = pos.y;


    // TODO: eventually move away from hardcoding the username length through using env to retrieve it
    prompt_length = 7 + strlen(dir_history.data[current_dir]) + 1;

    prompt_end_y = prompt_start_y + (prompt_length / win_size_x()); 
}

void read_line(char** buffer, size_t* size, ssize_t* nread)
{
    if ((*nread = getline(buffer, size, stdin)) != -1)
    {
        // If read line isn't just a newline, add it to the line_history s_vector
        if (**buffer != '\n')
        {
            buf_add_string(&line_history, *buffer, *nread);
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
    if (c == ' ' || c == '\t' || c == '\n' || c == '\0') { return WHITESPACE; }
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
        //
        // for (size_t i = 0; i < tokens->size; i++)
        // {
        //     if (tokens->data[i])
        //         printf("%lu: %s\n", i, tokens->data[i]);
        // }

        return true;
    }
    printf("tokenized\n");
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

        if ((!strcmp("<", tokens->data[i]) )||
            (!strcmp(">", tokens->data[i]) && (redir = 1)) ||
            (!strcmp("2>", tokens->data[i]) && (redir = 2)))
        {

            printf("strcmp found redir: %s", tokens->data[i]);
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

bool is_alpha_numeric_symbolic(char c)
{
    return (c >= 32 && c <= 126);
}


void refresh_interactive_line()
{
    int interactive_line_start_y = prompt_end_y;
    if (interactive_line_start_y > win_size_y())
    {
        interactive_line_start_y = win_size_y();
        prompt_start_y--;
        prompt_end_y--;
    }
    int interactive_line_start_x = prompt_length % (win_size_x());

    move_cursor(interactive_line_start_y, interactive_line_start_x);

    // erase from cursor to end of screen
    printf("\033[J");

    printf("%.*s", (int)interactive_line.size, interactive_line.data);

    int raw_cursor_displacement_x = interactive_line_start_x + (int)interactive_line.cursor_pos;
    int final_cursor_x = (raw_cursor_displacement_x - 1) % (win_size_x()) + 1;
    int final_cursor_y = interactive_line_start_y + ((raw_cursor_displacement_x - 1) / win_size_x());


    if (
            interactive_line_start_y + (((int)interactive_line.size + interactive_line_start_x - 1) / win_size_x()) == win_size_y() + 1
        )
    {
        printf("\n");
        // final_cursor_y = win_size_y();
        final_cursor_y--;
        prompt_start_y--;
        prompt_end_y--;
    }
    move_cursor(final_cursor_y, final_cursor_x);

    fflush(stdout);
}


void print_key_info(char* buf, int read_bytes)
{
    printf("\nRead bytes: %d\n", read_bytes);
    printf("Numerical representation: ");
    for (int i = 0; i < 8; i++)
    {
        printf("%d ", (int)buf[i]);
    }
    printf("\nChar representation: ");

    for (int i = 0; i < read_bytes; i++)
    {
        printf("%c ", buf[i]);
    }
    printf("\n");
}

void send_line()
{
    s_vector tokens = {0};

    putc('\n', stdout);

    // turn back on echo so child process shows input correctly
    set_term_echo_and_canonical(true);

    bool success = false;
    if (tokenize(&tokens, interactive_line.data, interactive_line.size + 1))
    {
        if (parse_tokens(&tokens))
            success = true;
    }

    set_term_echo_and_canonical(false);

    if (success && ( line_history.size == 0 || strcmp(interactive_line.data, line_history.data[line_history.size - 1]) ))
        add_string(&line_history, interactive_line.data, true);

    clear_line(&interactive_line);
    refresh_prompt(true);
}

void clear_screen()
{
    move_cursor(1, 1);
    print_prompt();
    prompt_start_y = 1;
    prompt_end_y = prompt_start_y + (prompt_length / win_size_x()); 
    printf("\033[J");
}

typedef enum
{
    KEY_UNASSIGNED,
    KEY_ALPHA_NUM_SYMBOL,
    KEY_ENTER,

    KEY_BACKSPACE,
    KEY_C_BACKSPACE,
    KEY_DELETE,

    KEY_LEFT,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_UP,

    KEY_C_LEFT,
    KEY_C_RIGHT,
    KEY_C_UP,
    KEY_C_DOWN,

    KEY_C_AT,
    KEY_C_A,
    KEY_C_B,
    KEY_C_C,
    KEY_C_D,
    KEY_C_E,
    KEY_C_F,
    KEY_C_G,
    KEY_C_H,
    KEY_C_I,
    KEY_C_J,
    KEY_C_K,
    KEY_C_L,
    KEY_C_M,
    KEY_C_N,
    KEY_C_O,
    KEY_C_P,
    KEY_C_Q,
    KEY_C_R,
    KEY_C_S,
    KEY_C_T,
    KEY_C_U,
    KEY_C_V,
    KEY_C_W,
    KEY_C_X,
    KEY_C_Y,
    KEY_C_Z,
    KEY_C_LBRACKET,
    KEY_C_BACKSLASH,
    KEY_C_RBRACKET,
    KEY_C_CARET,
    KEY_C_UNDERSCORE,

} KEY;

KEY get_input(char* buf)
{
    int read_bytes = read(STDIN_FILENO, buf, 16);
    if (read_bytes == -1)
    {
        perror("Read error");
        exit(EXIT_FAILURE);
    }

    // print_key_info(buf, read_bytes);

    if (read_bytes == 1 && is_alpha_numeric_symbolic(buf[0])) { return KEY_ALPHA_NUM_SYMBOL; }
    else if (buf[0] == '\000') return KEY_C_AT;
    else if (buf[0] == '\001') return KEY_C_A;
    else if (buf[0] == '\002') return KEY_C_B;
    else if (buf[0] == '\003') return KEY_C_C;
    else if (buf[0] == '\004') return KEY_C_D;
    else if (buf[0] == '\005') return KEY_C_E;
    else if (buf[0] == '\006') return KEY_C_F;
    else if (buf[0] == '\007') return KEY_C_G;
    else if (buf[0] == '\010') return KEY_BACKSPACE; // C-H
    else if (buf[0] == '\177') return KEY_BACKSPACE;
    else if (buf[0] == '\011') return KEY_C_I;
    else if (buf[0] == '\012') return KEY_ENTER; // C-J
    else if (buf[0] == '\013') return KEY_C_K;
    else if (buf[0] == '\014') return KEY_C_L;
    else if (buf[0] == '\015') return KEY_C_M;
    else if (buf[0] == '\016') return KEY_C_N;
    else if (buf[0] == '\017') return KEY_C_O;
    else if (buf[0] == '\020') return KEY_C_P;
    else if (buf[0] == '\021') return KEY_C_Q;
    else if (buf[0] == '\022') return KEY_C_R;
    else if (buf[0] == '\023') return KEY_C_S;
    else if (buf[0] == '\024') return KEY_C_T;
    else if (buf[0] == '\025') return KEY_C_U;
    else if (buf[0] == '\026') return KEY_C_V;
    else if (buf[0] == '\027') return KEY_C_W;
    else if (buf[0] == '\030') return KEY_C_X;
    else if (buf[0] == '\031') return KEY_C_Y;
    else if (buf[0] == '\032') return KEY_C_Z;
    else if (buf[0] == '\034') return KEY_C_BACKSLASH;
    else if (buf[0] == '\035') return KEY_C_RBRACKET;
    else if (buf[0] == '\036') return KEY_C_CARET;
    else if (buf[0] == '\037') return KEY_C_UNDERSCORE;
    else if (buf[0] == 127) return KEY_DELETE;
    else if (!strcmp(buf, "\033[A"))    return KEY_UP;
    else if (!strcmp(buf, "\033[B"))    return KEY_DOWN;
    else if (!strcmp(buf, "\033[C"))    return KEY_RIGHT;
    else if (!strcmp(buf, "\033[D"))    return KEY_LEFT;
    else if (!strcmp(buf, "\033[1;5A")) return KEY_C_UP;
    else if (!strcmp(buf, "\033[1;5B")) return KEY_C_DOWN;
    else if (!strcmp(buf, "\033[1;5C")) return KEY_C_RIGHT;
    else if (!strcmp(buf, "\033[1;5D")) return KEY_C_LEFT;
    else if (!strcmp(buf, "\033[3~"))   return KEY_DELETE;
    else { return KEY_UNASSIGNED; }
}

KEY previous_key = KEY_UNASSIGNED;

void handle_input()
{
    char input[16] = {0};
    KEY key_type = get_input(input);

    switch (key_type)
    {
        case KEY_ENTER:
            send_line(); break;
        case KEY_C_L:
            clear_screen(); break;
        case KEY_C_P:
            backward_history_search(); break;
        case KEY_C_W:
            delete_word_backwards(&interactive_line); break;
        case KEY_ALPHA_NUM_SYMBOL:
            insert_character(&interactive_line, input[0]); break;
        case KEY_LEFT:
            move_line_cursor_x(&interactive_line, -1); break;
        case KEY_RIGHT:
            move_line_cursor_x(&interactive_line, 1); break;
        case KEY_DOWN:
            forward_history_search(); break;
        case KEY_UP:
            backward_history_search(); break;
        case KEY_C_LEFT:
            break;
        case KEY_C_RIGHT:
            break;
        case KEY_C_UP:
            break;
        case KEY_C_DOWN:
            break;
        case KEY_BACKSPACE:
            remove_character(&interactive_line); break;
        case KEY_DELETE:
            remove_character_forward(&interactive_line); break;
        case KEY_C_BACKSPACE:
            delete_word_backwards(&interactive_line); break;
        case KEY_UNASSIGNED:
            break;
    }

    if (key_type != KEY_UNASSIGNED)
    {
        previous_key = key_type;
    }
}

void run()
{
    refresh_prompt(true);
    while (true)
    {
        handle_input();
        refresh_interactive_line();
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

    initscr();

    initialize_line(&interactive_line);

    add_path(&paths, "/bin/");
    add_path(&paths, "/usr/local/bin/");

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    add_string(&dir_history, cwd, true);
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

void delete_word_backwards(line* l)
{
    bool found_non_whitespace = false;
    bool found_whitespace_after_non_white_space = false;

    char current_character = '\0';
    while(l->cursor_pos > 0)
    {
        current_character = l->data[l->cursor_pos - 1];

        if (current_character != ' ') { found_non_whitespace = true; }
        else if (found_non_whitespace) { found_whitespace_after_non_white_space = true; }

        remove_character(&interactive_line);
        if (found_non_whitespace && found_whitespace_after_non_white_space)
        {
            break;
        }
    }
}

void remove_character_forward(line* l)
{
    if (l->cursor_pos == l->size) { return; }

    move_line_cursor_x(l, 1);
    remove_character(l);
}

void backward_history_search()
{
    if (line_history.size == 0) { return; }
    bool previously_searched = (previous_key == KEY_UP || previous_key == KEY_C_P || previous_key == KEY_DOWN || previous_key == KEY_C_N);
    if (!previously_searched)
    {
        line_history_search_index = (ssize_t)line_history.size - 1;
        clear_line_and_free(&temp_line);
    }
    if (line_history_search_index == -1) { return; }

    // case 1: interactive line has no data
    //      don't set temp line, just go back in history by one
    // case 2: interactive line has data and temp line does not, and this is the first history search (initiate search)
    // cash 3: interactive line has data and temp line does not, but search was never initiated -> just go back in history by one
    // case 4: interactive line has data and so does temp (search pattern), search was initiated, follow through on search

    if (interactive_line.size == 0 || (!temp_line.size && previously_searched))
    {
        clear_line_and_free(&interactive_line);
        initialize_line_with_new_data(&interactive_line, line_history.data[line_history_search_index]);

        line_history_search_index--;
    }
    else if (interactive_line.size)
    {
        char* data_to_search = (previously_searched) ? temp_line.data : interactive_line.data;
        line_history_search_index = find_index_of_next_string_match(&line_history, line_history_search_index, data_to_search, true);
        if (line_history_search_index == -1) { return; }

        if (!previously_searched)
            initialize_line_with_new_data(&temp_line, interactive_line.data);

        clear_line_and_free(&interactive_line);
        initialize_line_with_new_data(&interactive_line, line_history.data[line_history_search_index]);


        line_history_search_index--;
    }
}

void forward_history_search()
{
//     if (line_history.size == 0) { return; }
//     bool previously_searched = (previous_key == KEY_UP || previous_key == KEY_C_P || previous_key == KEY_DOWN || previous_key == KEY_C_N);
//     if (!previously_searched) { return; }
//     if (line_history_search_index == line_history.size - 1) { return; }
//
//     if (temp_line.data == NULL)
//     {
//         // printf("oairenst\n");
//         // clear_line_and_free(&interactive_line);
//         // initialize_line_with_new_data(&interactive_line, line_history.data[line_history_search_index + 1]);
//
//         clear_line_and_free(&interactive_line);
//         initialize_line_with_new_data(&interactive_line, line_history.data[++line_history_search_index]);
//     }
//
//
}

ssize_t find_index_of_next_string_match(s_vector* haystack, ssize_t current_index, char* needle, bool search_backwards)
{
    assert(haystack && needle);
    if (strlen(needle) == 0) { return -1; }

    while (current_index >= 0 && current_index < haystack->size)
    {

        char* subset_index = strstr(haystack->data[current_index], needle);
        if (subset_index != NULL && strlen(needle) < strlen(haystack->data[current_index]))
            return current_index;

        if (search_backwards) 
            current_index--;

        if (!search_backwards)
            current_index++;
    }

    return -1;
}
