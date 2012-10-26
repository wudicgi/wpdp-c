#include "internal.h"
#include "malloc.h"

/**
 * 分配 n 字节的空间并进行清零。成功时返回指针，失败时返回 NULL。
 */
void *wpdp_malloc_zero(int n) {
    void *p = malloc((size_t)n);
    if (p) {
        memset(p, 0, (size_t)n);
    }
    return p;
}

/**
 * 重新为 p 分配 n 字节的空间 (结尾新增的空间不进行清零)。成功时返回指针，失败时返回 NULL。
 */
void *wpdp_realloc(void *p, int n) {
    return realloc(p, (size_t)n);
}

void wpdp_free(void *p) {
    if (p) {
        free(p);
        p = NULL;
    }
}
