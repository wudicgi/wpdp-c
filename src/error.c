#include "internal.h"
#include <stdarg.h>

char buf_err_msg[512];

void error_set_msg(char *format, ...) {
    va_list args;

    va_start(args, format);
    vsnprintf(buf_err_msg, 512, format, args);
    va_end(args);

    perror("Error message: ");
    perror(buf_err_msg);
    perror("\r\n");
}
