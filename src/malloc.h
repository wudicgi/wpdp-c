#ifndef _MALLOC_H_
#define _MALLOC_H_

#define wpdp_new_zero(struct_type, n_structs)   \
    ((struct_type*)wpdp_malloc_zero((int)sizeof(struct_type) * n_structs))

void *wpdp_malloc_zero(int n);

void *wpdp_realloc(void *p, int n);

void wpdp_free(void *p);

#endif // _MALLOC_H_
