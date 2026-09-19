#include "pi/c/terminal.h"

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

static struct termios g_saved;
static int            g_saved_valid = 0;

pi_term_size pi_term_get_size(void) {
    pi_term_size size;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        size.rows = ws.ws_row;
        size.cols = ws.ws_col;
    } else {
        size.rows = 24;
        size.cols = 80;
    }
    return size;
}

int pi_term_enable_raw(void) {
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &g_saved) != 0) {
        return -1;
    }
    g_saved_valid = 1;

    struct termios raw = g_saved;
    /* Disable canonical mode, echo, signals; keep output post-processing. */
    raw.c_lflag &= (unsigned)~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= (unsigned)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        g_saved_valid = 0;
        return -1;
    }
    return 0;
}

void pi_term_restore(void) {
    if (g_saved_valid) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
        g_saved_valid = 0;
    }
}

int pi_term_read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) {
        return (int)c;
    }
    return -1;
}
