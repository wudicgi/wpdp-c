#include "internal.h"

#define DEBUG_CURRENT_DIR   "D:/Projects/CodeBlocks/wpdp/bin/Debug/"

static char *_filename   = DEBUG_CURRENT_DIR "_test.5dp";
static char *_filename_m = DEBUG_CURRENT_DIR "_test.5dpi";
static char *_filename_i = DEBUG_CURRENT_DIR "_test.5dpm";

static char *_filename_l = DEBUG_CURRENT_DIR "_test_lookup.5dp";

bool file_exists(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

void test_create(void) {
    unlink(_filename);
    unlink(_filename_m);
    unlink(_filename_i);

    assert(file_exists(_filename) == false);
    assert(file_exists(_filename_m) == false);
    assert(file_exists(_filename_i) == false);

//    WPDP_File::create(_filename);

    assert(file_exists(_filename) == true);
    assert(file_exists(_filename_m) == true);
    assert(file_exists(_filename_i) == true);
}

int main(void) {
#ifndef BUILD_READONLY

    char buffer[8192];
    FILE *fp_tmp;
    int rc;

    char data_for_test[32] = "this is the data for test";

    fp_tmp = fopen(DEBUG_CURRENT_DIR "test.5dp", "wb");
    fclose(fp_tmp);
    fp_tmp = fopen(DEBUG_CURRENT_DIR "test.5dpm", "wb");
    fclose(fp_tmp);
    fp_tmp = fopen(DEBUG_CURRENT_DIR "test.5dpi", "wb");
    fclose(fp_tmp);

    WPIO_Stream *stream_c = file_open(DEBUG_CURRENT_DIR "test.5dp", WPIO_MODE_READ_WRITE);
    WPIO_Stream *stream_m = file_open(DEBUG_CURRENT_DIR "test.5dpm", WPIO_MODE_READ_WRITE);
    WPIO_Stream *stream_i = file_open(DEBUG_CURRENT_DIR "test.5dpi", WPIO_MODE_READ_WRITE);

    wpdp_create_stream(stream_c, stream_m, stream_i);

//    printf("debug end.\r\n"); system("pause"); return 0;

    WPDP *dp;
    rc = wpdp_open_stream(stream_c, stream_m, stream_i, WPDP_MODE_READONLY, &dp);
    RETURN_VAL_IF_NON_ZERO(rc);

//    printf("debug end.\r\n"); system("pause"); return 0;

    WPDP_Entry_Attributes *attrs;

    // 1
    attrs = wpdp_entry_attributes_create();
    rc = wpdp_entry_attributes_add(attrs, wpdp_entry_attribute_create(
        wpdp_string_from_cstr("name_1\0test"),
        wpdp_string_from_cstr("value_1_1\0test"),
        true
    ));
    RETURN_VAL_IF_NON_ZERO(rc);
    rc = wpdp_add(dp, data_for_test, strlen(data_for_test), attrs);
    RETURN_VAL_IF_NON_ZERO(rc);

    // 2
    attrs = wpdp_entry_attributes_create();
    rc = wpdp_entry_attributes_add(attrs, wpdp_entry_attribute_create(
        wpdp_string_from_cstr("name_1"),
        wpdp_string_from_cstr("value_1_5"),
        true
    ));
    RETURN_VAL_IF_NON_ZERO(rc);
    rc = wpdp_add(dp, data_for_test, strlen(data_for_test), attrs);
    RETURN_VAL_IF_NON_ZERO(rc);

    // 3
    attrs = wpdp_entry_attributes_create();
    rc = wpdp_entry_attributes_add(attrs, wpdp_entry_attribute_create(
        wpdp_string_from_cstr("name_1"),
        wpdp_string_from_cstr("value_1_2"),
        true
    ));
    RETURN_VAL_IF_NON_ZERO(rc);
    rc = wpdp_add(dp, data_for_test, strlen(data_for_test), attrs);
    RETURN_VAL_IF_NON_ZERO(rc);
/*
    WPIO_Stream *fp = file_open(DEBUG_CURRENT_DIR "test.png", WPIO_MODE_READ_ONLY);
    wpio_seek(fp, 0, SEEK_END);
    int64_t length = wpio_tell(fp);
    wpio_seek(fp, 0, SEEK_SET);

    wpdp_begin(dp, attrs, length);
    while (!wpio_eof(fp)) {
        int len = (int)wpio_read(fp, buffer, 8192);
        if (len == 0) {
            break;
        }
        wpdp_transfer(dp, buffer, len);
    }
    wpdp_commit(dp);
*/
#endif

    printf("debug finished.\r\n");
    system("pause");
    return 0;

/*
    WPIO_Stream *stream_1;
    WPIO_Stream *stream_2;
    char buffer[8192];
    size_t length;

    stream_1 = file_open("D:/Projects/CodeBlocks/WPDP/bin/Debug/install_1.txt", WPIO_MODE_READ_ONLY);
    if (stream_1 == NULL) {
        printf("stream_1 open failed.\n");
        return -1;
    }

    stream_2 = wpes_open("D:/Projects/CodeBlocks/WPDP/bin/Debug/install_2.txt", WPIO_MODE_READ_WRITE);
    if (stream_2 == NULL) {
        printf("stream_2 open failed.\n");
        return -1;
    }

    wpio_seek(stream_2, 0, SEEK_SET);

    while (!wpio_eof(stream_1)) {
        length = wpio_read(stream_1, buffer, 8192);
        if (length == 0) {
            break;
        }
        wpio_write(stream_2, buffer, length);
    }

    wpio_close(stream_1);
    wpio_close(stream_2);
*/
    /*
    structField **fields;
    int i;

    fields = (structField**)malloc(sizeof(structField*)*3);
    for (i = 0; i < 3; i++) {
        fields[i] = (structField*)malloc(sizeof(structField));
        fields[i]->name = "test";
    }
    */

    printf("Hello World!\n");
    return 0;
}

