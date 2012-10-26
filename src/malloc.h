#ifndef _MALLOC_H_
#define _MALLOC_H_

/**
 * 分配 n_structs 个 struct_type 结构体的空间并进行清零。成功时返回指针，失败时返回 NULL。
 */
#define wpdp_new_zero(struct_type, n_structs)   \
    ((struct_type*)wpdp_malloc_zero((int)sizeof(struct_type) * n_structs))

void *wpdp_malloc_zero(int n);

void *wpdp_realloc(void *p, int n);

void wpdp_free(void *p);

#endif // _MALLOC_H_
