#ifndef SHELL_H
#define SHELL_H

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
#include <termios.h>
#include <sys/ioctl.h>

#include "s_vector.h"
#include "line.h"
#include "cursor.h"

typedef struct command
{
    size_t args_start;
    size_t args_end;
    char* stdin_redir;
    char* stdout_redir;
    char* stderr_redir;
    struct command* pipe; // Piped command
    bool bg; // Foreground or background cmd

} command;

void add_string(s_vector* lines, char* buffer, bool copy);
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread);
void erase(s_vector* vec, int pos);
void add_path(s_vector*, char* path_name);
void execute_bin(const command* command, s_vector* tokens);
int num_args(const command* command);
int file_status(char* path_name);
int count_digits(int n);

void cd(const command* command, s_vector* tokens);
void prevd(const command* command);
void nextd(const command* command);
void dirh(const command* command);
void path(const command* command, s_vector* tokens);

void delete_word_backwards(line* l);

void clear_screen();

void handle_command(const command* command, s_vector* args);
void print_prompt();
void refresh_prompt(bool flush);
void read_line(char** buffer, size_t* size, ssize_t* nread);
void init(int argc, char* argv[]);
void run();

extern line interactive_line;
extern s_vector paths;
extern s_vector lines;
extern s_vector dir_history;
extern size_t current_dir;

extern size_t prompt_start_y;
extern size_t prompt_end_y;
extern size_t prompt_length;

#endif
