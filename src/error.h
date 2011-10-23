#ifndef _ERROR_H_
#define _ERROR_H_

#define RETURN_VAL_IF_NON_ZERO(val)     do { \
        if (val) { return val; } \
    } while (0)

#define RETURN_CODE(rc)     (rc)

#define CHECK_IS_READ_EXACTLY(actual, expected, error)
#define CHECK_IS_WRITE_EXACTLY(actual, expected, error)

#define IN_ARRAY_1(needle, val_1)  \
    (needle == val_1)

#define IN_ARRAY_2(needle, val_1, val_2)  \
    (needle == val_1) || (needle == val_2)

#define IN_ARRAY_3(needle, val_1, val_2, val_3)  \
    (needle == val_1) || (needle == val_2) || (needle == val_3)

#define IN_ARRAY_4(needle, val_1, val_2, val_3, val_4)  \
    (needle == val_1) || (needle == val_2) || (needle == val_3) || (needle == val_4)

#define IN_ARRAY_5(needle, val_1, val_2, val_3, val_4, val_5)  \
    (needle == val_1) || (needle == val_2) || (needle == val_3) || (needle == val_4) || (needle == val_5)

#define trace(msg, ...) \
    printf("In %s, Line %d: " msg "\r\n", __FILE__, __LINE__, __VA_ARGS__)

#undef trace
#define trace(msg, ...)

/*
#define CHECK_IS_READ_EXACTLY(actual, expected, error) \
    if (actual != expected) { \
        g_set_error(error, WPDP_ERROR, WPDP_ERROR_STREAM_OPERATION, \
                    "Failed to read %d bytes (%d bytes read actually)", \
                    expected, actual); \
        return FALSE; \
    }

#define CHECK_IS_WRITE_EXACTLY(actual, expected, error) \
    if (actual != expected) { \
        g_set_error(error, WPDP_ERROR, WPDP_ERROR_STREAM_OPERATION, \
                    "Failed to write %d bytes (%d bytes written actually)", \
                    expected, actual); \
        return FALSE; \
    }
*/

// rc: return code
#define WPDP_OK                                 0
#define WPDP_ERROR                              1
#define WPDP_ERROR_NOT_COMPATIBLE               2
#define WPDP_ERROR_BAD_FUNCTION_CALL            3
#define WPDP_ERROR_OUT_OF_BOUNDS                4
#define WPDP_ERROR_INVALID_ARGUMENT             5
#define WPDP_ERROR_INVALID_ATTRIBUTE_NAME       6
#define WPDP_ERROR_INVALID_ATTRIBUTE_VALUE      7
#define WPDP_ERROR_FILE_OPEN                    8
#define WPDP_ERROR_EXCEED_LIMIT                 9
#define WPDP_ERROR_INTERNAL                     10
#define WPDP_ERROR_FILE_BROKEN                  11
#define WPDP_ERROR_STREAM_OPERATION             12

void error_set_msg(char *format, ...);

#endif // _ERROR_H_
