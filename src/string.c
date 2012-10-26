#include "internal.h"

WPDP_String_Builder *wpdp_string_builder_create(int init_capacity) {
    WPDP_String_Builder *builder = wpdp_new_zero(WPDP_String_Builder, 1);

    builder->str = wpdp_malloc_zero(init_capacity + 1);
    builder->length = 0;
    builder->capacity = init_capacity;

    return builder;
}

WPDP_String_Builder *wpdp_string_builder_build(void *str, int len) {
    WPDP_String_Builder *builder = wpdp_new_zero(WPDP_String_Builder, 1);

    builder->str = str;
    builder->length = len;
    builder->capacity = len;

    return builder;
}

int wpdp_string_builder_append(WPDP_String_Builder *builder, void *data, int len) {
    if (len > (builder->capacity - builder->length)) {
        int new_capacity = builder->capacity * 2;
        if (new_capacity < (builder->length + len)) {
            new_capacity = (builder->length + len);
        }
        builder->capacity = new_capacity;
        builder->str = wpdp_realloc(builder->str, builder->capacity + 1);
        *((char *)builder->str + builder->capacity) = '\0';
    }

    memcpy(builder->str + builder->length, data, (size_t)len);
    builder->length += len;

    return WPDP_OK;
}

int wpdp_string_builder_free(WPDP_String_Builder *builder) {
    wpdp_free(builder->str);
    wpdp_free(builder);

    return WPDP_OK;
}

WPDP_String *wpdp_string_direct(void *str, int len) {
    WPDP_String *wpdp_str = wpdp_new_zero(WPDP_String, 1);
    wpdp_str->len = len;
    wpdp_str->str = str;

    return wpdp_str;
}

WPDP_String *wpdp_string_create(const char *str, int len) {
    WPDP_String *wpdp_str = wpdp_new_zero(WPDP_String, 1);
    wpdp_str->len = len;
    wpdp_str->str = wpdp_malloc_zero(len + 1);
    memcpy(wpdp_str->str, str, (size_t)len);

    return wpdp_str;
}

WPDP_String *wpdp_string_from_cstr(const char *str) {
    return wpdp_string_create(str, (int)strlen(str));
}

int wpdp_string_compare(WPDP_String *str_1, WPDP_String *str_2) {
    int retval = memcmp(str_1->str, str_2->str,
        (size_t)((str_1->len < str_2->len) ? str_1->len : str_2->len));

    if (retval) {
        return retval;
    } else {
        return (str_1->len - str_2->len);
    }
}

int wpdp_string_free(WPDP_String *str) {
    wpdp_free(str->str);
    wpdp_free(str);

    return WPDP_OK;
}
