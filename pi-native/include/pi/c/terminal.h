/*
 * C boundary layer: direct terminal control via termios.
 *
 * This lives in C on purpose. Raw-mode terminal handling is a thin layer over
 * OS syscalls (tcgetattr/tcsetattr, ioctl(TIOCGWINSZ)); there is nothing for
 * C++ abstractions to buy us here, and a stable C surface is the natural FFI
 * boundary.
 */
#ifndef PI_C_TERMINAL_H
#define PI_C_TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int rows;
    int cols;
} pi_term_size;

/* Query the controlling terminal size. Falls back to 24x80 when unavailable. */
pi_term_size pi_term_get_size(void);

/* Switch the terminal to raw mode. Returns 0 on success, -1 otherwise.
 * The previous termios state is stashed internally for pi_term_restore(). */
int pi_term_enable_raw(void);

/* Restore the terminal state captured by the last pi_term_enable_raw(). */
void pi_term_restore(void);

/* Read a single byte from stdin (blocking). Returns the byte, or -1 on EOF. */
int pi_term_read_key(void);

#ifdef __cplusplus
}
#endif

#endif /* PI_C_TERMINAL_H */
