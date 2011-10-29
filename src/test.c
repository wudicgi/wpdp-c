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

