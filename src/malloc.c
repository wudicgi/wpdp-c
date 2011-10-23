#include "internal.h"
#include "malloc.h"

void *wpdp_malloc_zero(int n) {
    void *p = malloc((size_t)n);
    if (p) {
        memset(p, 0, (size_t)n);
    }
    return p;
}

void *wpdp_realloc(void *p, int n) {
    return realloc(p, (size_t)n);
}

void wpdp_free(void *p) {
    if (p) {
        free(p);
        p = NULL;
    }
}
