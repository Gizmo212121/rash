#include "../include/cursor.h"

struct winsize win_size;
struct termios term_settings;

int win_size_x(void) { return win_size.ws_col; }
int win_size_y(void) { return win_size.ws_row; }

void move_cursor(int y, int x)
{
    assert(y > 0 && x > 0);
    assert(y <= win_size_y() && x <= win_size_x());

    printf("\033[%d;%dH", y, x);
}

cursor_pos get_cursor()
{
    // Write term code and get response
    write(STDOUT_FILENO, "\033[6n", 4);
    char answer[16] = {0};
    int bytes_read = read(STDIN_FILENO, answer, 16);
    if (bytes_read == -1)
    {
        perror("get_cursor read");
        exit(EXIT_FAILURE);
    }

    // Answer is in the form \e[y;xR
    cursor_pos pos = {0};
    bool found_x_delim = false;
    bool found_y_delim = false;
    for (int i = 2; i < 16; i++)
    {
        if (!found_y_delim && answer[i] == ';')
        {
            found_y_delim = true;
            continue;
        }
            
        if (answer[i] == 'R')
        {
            found_x_delim = true;
            break;
        }

        if (!found_y_delim)
            pos.y = pos.y * 10 + answer[i] - '0';

        if (found_y_delim && !found_x_delim)
            pos.x = pos.x * 10 + answer[i] - '0';
    }

    return pos;
}

// Initialize term settings, turning off echo and canonical mode
// This allows grabbing of input without return 
void init_canonical_term()
{
    int result = tcgetattr(STDIN_FILENO, &term_settings);
    if (result == -1)
    {
        perror("tcgetattr\n");
        exit(EXIT_FAILURE);
    }

    term_settings.c_lflag &= ~ECHO;
    term_settings.c_lflag &= ~ICANON;
    term_settings.c_cc[VMIN] = 1;
    term_settings.c_cc[VTIME] = 0;

    result = tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
    if (result == -1)
    {
        perror("tcsetattr\n");
        exit(EXIT_FAILURE);
    }
}

void set_term_echo_and_canonical(bool enable)
{
    if (enable)
    {
        term_settings.c_lflag |= ECHO;
        term_settings.c_lflag |= ICANON;
    }
    else
    {
        term_settings.c_lflag &= ~ECHO;
        term_settings.c_lflag &= ~ICANON;
    }

    int result = tcsetattr(STDIN_FILENO, TCSADRAIN, &term_settings);
    if (result == -1)
    {
        perror("tcsetattr\n");
        exit(EXIT_FAILURE);
    }
}

void recalculate_window_dimensions(int signum)
{
    UNUSED(signum);
    ioctl( 0, TIOCGWINSZ, &win_size );
}

void initscr()
{
    init_canonical_term();
    ioctl( 0, TIOCGWINSZ, &win_size );
    signal(SIGWINCH, recalculate_window_dimensions);
}
