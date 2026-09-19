/*
 * C boundary layer: subprocess execution.
 *
 * fork/exec + pipe plumbing is exactly the kind of syscall-level work C is
 * good at and where manual control (fd lifetimes, exec semantics) matters.
 * The C++ side wraps the result buffer in RAII; the syscalls stay here.
 */
#ifndef PI_C_SUBPROCESS_H
#define PI_C_SUBPROCESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   exit_code;   /* process exit status, or -1 on spawn failure */
    char *output;      /* malloc'd, NUL-terminated; caller frees via pi_subprocess_free */
    size_t output_len;
} pi_subprocess_result;

/*
 * Run `command` through `/bin/sh -c`, capturing stdout+stderr (merged).
 * Blocks until the child exits. `output` is always allocated (possibly empty).
 */
pi_subprocess_result pi_subprocess_run(const char *command);

void pi_subprocess_free(pi_subprocess_result *result);

#ifdef __cplusplus
}
#endif

#endif /* PI_C_SUBPROCESS_H */
