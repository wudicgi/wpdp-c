#include "internal.h"

/**
 * 当前 WPDP 库的版本
 */
#define _LIBRARY_VERSION "0.1.0.0-dev"

/**
 * 当前库所依赖的库的版本
 */
#define _DEPEND_WPIO_VERSION "0.1.0-dev"

/**
 * 当前库的 INT32 限制所允许的最大文件大小
 *
 *   PHP_INT_MAX = 2^31 -    1 = 2147483647 = 2GB - 1B
 * _FILESIZE_MAX = 2^31 - 2^25 = 2113929216 = 2GB - 32MB = 1.96875GB
 *
 *     C_INT_MAX = 2^63 -    1 = 9223372036854775807 = 8EB - 1B
 * _FILESIZE_MAX = ...         =        137438953472 = 128GB
 */
#define _FILESIZE_MAX 2113929216

#define CHECK_DEPS() \
    if (check_dependencies()) { \
        return WPDP_ERROR; \
    }

#define CHECK_CAPS(stream, cap) \
    if (check_capabilities(stream, cap)) { \
        return WPDP_ERROR; \
    }

/**
 * 流的能力检查常量
 */
enum {
    CAPABILITY_READ = 0x01,            // 检查是否可读
    CAPABILITY_WRITE = 0x02,           // 检查是否可写
    CAPABILITY_SEEK = 0x04,            // 检查是否可定位
    CAPABILITY_READ_SEEK = 0x05,       // 检查是否可读且可定位
    CAPABILITY_READ_WRITE_SEEK = 0x07  // 检查是否可读、可写且可定位
};

typedef struct _SectionFilenames {
    char    *contents;
    char    *metadata;
    char    *indexes;
} SectionFilenames;

static int check_dependencies(void);
static int check_capabilities(WPIO_Stream *stream, int capabilities);

/*
void wpdp_create_files(const char *filename) {
    WPDP *wpdp;

    wpdp = (WPDP*)malloc(sizeof(WPDP));
}

WPDP_SECTION_FILENAMES *wpdp_get_section_filenames(const char *filename) {
    char filename_temp[256];
    WPDP_SECTION_FILENAMES *filenames;

    strcpy(filename_temp, filename);

    filenames = (WPDP_SECTION_FILENAMES*)malloc(sizeof(WPDP_SECTION_FILENAMES));

    if (strstr(filename_temp, ".5dp") == (filename_temp + strlen(filename_temp) - 4)) {
        filename_temp[strlen(filename_temp) - 4] = '\0';
    }

    return filenames;
}
*/

WPDP_API char *wpdp_library_version(void) {
    return _LIBRARY_VERSION;
}

WPDP_API bool wpdp_library_compatible_with(const char *version) {
    return true;
}

/**
 * 打开数据堆
 *
 * @param stream_c 内容文件操作对象
 * @param stream_m 元数据文件操作对象
 * @param stream_i 索引文件操作对象
 * @param mode     打开模式
 */
WPDP_API int wpdp_open_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                              WPIO_Stream *stream_i, WPDP_OpenMode mode, WPDP **dp_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    WPDP *dp = NULL;
    StructHeader *header = NULL;

    // 检查所依赖库的版本
    CHECK_DEPS();

    // 检查打开模式参数
    if (mode != WPDP_MODE_READONLY && mode != WPDP_MODE_READWRITE) {
        error_set_msg("Invalid open mode: %d", mode);
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

//#ifdef BUILD_READONLY
    if (mode != WPDP_MODE_READONLY) {
        error_set_msg("This is a readonly build of WPDP");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }
//#endif

    // 检查内容流的可读性与可定位性
    CHECK_CAPS(stream_c, CAPABILITY_READ_SEEK);

    // 检查元数据流的可读性与可定位性
    if (stream_m != NULL) {
        CHECK_CAPS(stream_m, CAPABILITY_READ_SEEK);
    }

    // 检查索引流的可读性与可定位性
    if (stream_i != NULL) {
        CHECK_CAPS(stream_i, CAPABILITY_READ_SEEK);
    }

    // 读取文件的头信息
    header = wpdp_new_zero(StructHeader, 1);
    wpio_seek(stream_c, 0, SEEK_SET);
    struct_read_header(stream_c, &header);

    if (header->version != HEADER_THIS_VERSION) {
        error_set_msg("The specified data pile is not supported by the WPDP version %s",
                      "***"/*wpdp_library_version()*/);
        return WPDP_ERROR_NOT_COMPATIBLE;
    }

    // 检查文件限制类型
    if (header->limit != HEADER_LIMIT_INT32) {
        error_set_msg("This implemention supports only int32 limited file");
        return WPDP_ERROR_FILE_OPEN;
    }

    dp = wpdp_new_zero(WPDP, 1);

    dp->_file_version = header->version;
    dp->_file_type = header->type;
    dp->_file_limit = header->limit;

    dp->_open_mode = mode;

    dp->_space_available = 0;

    switch (header->type) {
        case HEADER_TYPE_COMPOUND:
            section_contents_open(stream_c, dp->_open_mode, &dp->_contents);
//            dp->metadata = metadata_open(stream_c);
//            dp->indexes = indexes_open(stream_c);
            break;
        /*
        case HEADER_TYPE_LOOKUP:
            dp->contents = NULL;
            dp->metadata = metadata_open(stream_c);
            dp->indexes = indexes_open(stream_c);
            break;
        */
        case HEADER_TYPE_CONTENTS:
            section_contents_open(stream_c, dp->_open_mode, &dp->_contents);
            section_metadata_open(stream_m, dp->_open_mode, &dp->_metadata);
            section_indexes_open(stream_i, dp->_open_mode, &dp->_indexes);
            break;
        default:
            error_set_msg("The file must be a compound, lookup or contents file");
            return WPDP_ERROR_FILE_OPEN;
            break;
    }

    dp->_opened = true;

    *dp_out = dp;

    return WPDP_OK;
}

/**
 * 创建数据堆
 *
 * @param stream_c  内容文件操作对象
 * @param stream_m  元数据文件操作对象
 * @param stream_i  索引文件操作对象
 */
WPDP_API int wpdp_create_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                WPIO_Stream *stream_i) {
    int rc;

    // 检查所依赖库的版本
    CHECK_DEPS();

    // 检查流的可读性、可写性与可定位性
    CHECK_CAPS(stream_c, CAPABILITY_READ_WRITE_SEEK);
    CHECK_CAPS(stream_m, CAPABILITY_READ_WRITE_SEEK);
    CHECK_CAPS(stream_i, CAPABILITY_READ_WRITE_SEEK);

/*
    rc = contents_create(stream_c);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = metadata_create(stream_m);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = indexes_create(stream_i);
    RETURN_VAL_IF_NON_ZERO(rc);
*/

    return WPDP_OK;
}

/**
 * 关闭当前打开的数据堆
 */
WPDP_API int wpdp_close(WPDP *dp) {
    if (!dp->_opened) {
        error_set_msg("The data pile has already closed");
        return WPDP_ERROR_BAD_FUNCTION_CALL;
    }

/*
    if (dp->_open_mode != WPDP_MODE_READONLY) {
        wpdp_flush(dp);
    }
*/

    dp->_contents = NULL;
    dp->_metadata = NULL;
    dp->_indexes = NULL;

    free(dp);

    return WPDP_OK;
}

/**
 * 获取当前数据堆文件的版本
 *
 * @return 当前数据堆文件的版本
 */
WPDP_API void *wpdp_file_info(WPDP *dp) {
    return NULL;
}

WPDP_API int64_t wpdp_file_space_used(WPDP *dp) {
    int64_t length = 0;

    switch (dp->_file_type) {
        case HEADER_TYPE_CONTENTS:
            length += HEADER_BLOCK_SIZE * 3;
            length += section_contents_get_section_length(dp->_contents);
            length += section_metadata_get_section_length(dp->_metadata);
//            length += dp->_indexes->getSectionLength();
            if (length % BASE_BLOCK_SIZE != 0) {
                length += BASE_BLOCK_SIZE - (length % BASE_BLOCK_SIZE);
            }
            break;
        case HEADER_TYPE_COMPOUND:
            length += HEADER_BLOCK_SIZE;
            length += section_contents_get_section_length(dp->_contents);
            length += section_metadata_get_section_length(dp->_metadata);
//            length += $this->_indexes->getSectionLength();
            break;
        case HEADER_TYPE_LOOKUP:
            length += HEADER_BLOCK_SIZE;
            length += section_metadata_get_section_length(dp->_metadata);
//            length += $this->_indexes->getSectionLength();
            break;
    }

    return length;
}

WPDP_API int64_t wpdp_file_space_available(WPDP *dp) {
    return _FILESIZE_MAX - wpdp_file_space_used(dp);
}

/**
 * 获取条目迭代器
 *
 * @return WPDP_Iterator 对象
 */
WPDP_API int wpdp_iterator_init(WPDP *dp, WPDP_Iterator **iterator_out) {
    PacketMetadata *meta_first;
    WPDP_Iterator *iterator;

    section_metadata_get_first(dp->_metadata, &meta_first);

    iterator = wpdp_new_zero(WPDP_Iterator, 1);
    iterator->dp = dp;
    iterator->first = meta_first;
    iterator->current = meta_first;

    *iterator_out = iterator;

    return WPDP_OK;
}

WPDP_API int wpdp_iterator_entry(WPDP_Iterator *iterator, WPDP_Entry **entry_out) {
    WPDP_Entry *entry;

    entry = wpdp_entry_create(iterator->dp, iterator->current);

    *entry_out = entry;

    return WPDP_OK;
}

WPDP_API int wpdp_iterator_next(WPDP_Iterator *iterator) {
    PacketMetadata *meta_next;
    section_metadata_get_next(iterator->dp->_metadata, iterator->current, &meta_next);

    iterator->current = meta_next;

    return WPDP_OK;
}

/**
 * 查询指定属性值的条目
 *
 * @param attr_name     属性名
 * @param attr_value    属性值
 *
 * @return 成功时返回 WPDP_Entries 对象，指定属性不存在索引时返回 false
 */
WPDP_API void *wpdp_query(WPDP *dp, const char *attr_name, const char *attr_value) {
    return NULL;
}

static int check_dependencies(void) {
    return WPDP_OK;
}

/**
 * 检查流是否具有指定的能力 (可读，可写或可定位)
 *
 * @param stream        流
 * @param capabilities  按位组合的 CAPABILITY 常量
 */
static int check_capabilities(WPIO_Stream *stream, int capabilities) {
    if ((capabilities & CAPABILITY_READ) && !wpio_is_readable(stream)) {
//                    "The specified stream is not readable");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

    if ((capabilities & CAPABILITY_WRITE) && !wpio_is_writable(stream)) {
//                    "The specified stream is not writable");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

    if ((capabilities & CAPABILITY_SEEK) && !wpio_is_seekable(stream)) {
//                    "The specified stream is not seekable");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

    return WPDP_OK;
};



/*
static bool _file_exists(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

typedef struct _Filenames {
    char    *contents;
    char    *metadata;
    char    *indexes;
} Filenames;

static Filenames *_get_filenames(const char *filename) {
    Filenames *filenames = wpdp_new_zero(Filenames, 1);

    filenames->contents = wpdp_malloc_zero(strlen(filename) + 5);
    strcpy(filenames->contents, filename);
    strcpy(filenames->contents + strlen(filename), ".5dp\0");

    filenames->metadata = wpdp_malloc_zero(strlen(filename) + 6);
    strcpy(filenames->metadata, filename);
    strcpy(filenames->metadata + strlen(filename), ".5dpm\0");

    filenames->indexes = wpdp_malloc_zero(strlen(filename) + 6);
    strcpy(filenames->indexes, filename);
    strcpy(filenames->indexes + strlen(filename), ".5dpi\0");

    return filenames;
}


WPDP_API int wpdp_open_file(const char *filename, WPDP_OpenMode mode, WPDP **dp_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    // 检查所依赖库的版本
    CHECK_DEPS();

    // 检查打开模式参数
    if (mode != WPDP_MODE_READONLY && mode != WPDP_MODE_READWRITE) {
        error_set_msg("Invalid open mode: %d", mode);
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

//#ifdef BUILD_READONLY
    if (mode != WPDP_MODE_READONLY) {
        error_set_msg("This is a readonly build of WPDP");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }
//#endif

    // 检查文件是否存在
    if (!_file_exists(filename)) {
        error_set_msg("File %s does not exist", filename);
        return WPDP_ERROR_FILE_OPEN;
    }

    // 检查文件是否可读
//    _check_readable(filename);

    Filenames *filenames = _get_filenames(filename);
    int filemode = (mode == WPDP_MODE_READWRITE) ? WPIO_MODE_READ_WRITE : WPIO_MODE_READ_ONLY;

    WPIO_Stream *stream_c = NULL;
    WPIO_Stream *stream_m = NULL;
    WPIO_Stream *stream_i = NULL;

    if (file_exists(filenames->contents)) {
        stream_c = file_open(filenames->contents, filemode);
    }
    if (file_exists(filenames->metadata)) {
        stream_m = file_open(filenames->metadata, filemode);
    }
    if (file_exists(filenames->indexes)) {
        stream_i = file_open(filenames->indexes, filemode);
    }

    return wpdp_open_stream(stream_c, stream_m, stream_i, mode, dp_out);
}





*/
