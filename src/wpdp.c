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

/**
 * 打开数据堆 (基于 WPIO_Stream)
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

#ifdef BUILD_READONLY
    if (mode != WPDP_MODE_READONLY) {
        error_set_msg("This is a readonly build of WPDP");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }
#endif

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

#ifndef BUILD_READONLY

    // 检查打开模式是否和数据堆类型及标志兼容
    if (mode == WPDP_MODE_READWRITE) {
        if (header->type == HEADER_TYPE_COMPOUND) {
            error_set_msg("The specified file is a compound one which is readonly");
            return WPDP_ERROR_FILE_OPEN;
        }

        if (header->type == HEADER_TYPE_LOOKUP) {
            error_set_msg("The specified file is a lookup one which is readonly");
            return WPDP_ERROR_FILE_OPEN;
        }

        if (header->type & HEADER_FLAG_READONLY) {
            error_set_msg("The specified file has been set to be readonly");
            return WPDP_ERROR_FILE_OPEN;
        }

        // 检查流的可写性
        CHECK_CAPS(stream_c, CAPABILITY_WRITE);
        CHECK_CAPS(stream_m, CAPABILITY_WRITE);
        CHECK_CAPS(stream_i, CAPABILITY_WRITE);
    }

#endif

    dp = wpdp_new_zero(WPDP, 1);

    dp->_file_version = header->version;
    dp->_file_type = header->type;
    dp->_file_limit = header->limit;

    dp->_open_mode = mode;
#ifndef BUILD_READONLY
    dp->_cache_mode = WPDP_CACHE_ENABLED;
    dp->_compression = WPDP_COMPRESSION_NONE;
    dp->_checksum = WPDP_CHECKSUM_NONE;
//    dp->attribute_indexes = array();
#endif

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

#ifndef BUILD_READONLY

/**
 * 创建数据堆 (基于 WPIO_Stream)
 *
 * @param stream_c  内容文件操作对象
 * @param stream_m  元数据文件操作对象
 * @param stream_i  索引文件操作对象
 */
WPDP_API int wpdp_create_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                     WPIO_Stream *stream_i) {
    // 检查所依赖库的版本
    CHECK_DEPS();

    // 检查流的可读性、可写性与可定位性
    CHECK_CAPS(stream_c, CAPABILITY_READ_WRITE_SEEK);
    CHECK_CAPS(stream_m, CAPABILITY_READ_WRITE_SEEK);
    CHECK_CAPS(stream_i, CAPABILITY_READ_WRITE_SEEK);

    section_contents_create(stream_c);
    section_metadata_create(stream_m);
    section_indexes_create(stream_i);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 合并数据堆 (基于 WPIO_Stream)
 *
 * @param stream_c  内容文件操作对象
 * @param stream_m  元数据文件操作对象
 * @param stream_i  索引文件操作对象
 */
WPDP_API int wpdp_compound_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                       WPIO_Stream *stream_i) {
//    StructHeader *header, *header_m, *header_i;
//    StructSection *section, *section_m, *section_i;

    // 检查所依赖库的版本
    CHECK_DEPS();

    // 检查内容流的可读性、可写性与可定位性
    CHECK_CAPS(stream_c, CAPABILITY_READ_WRITE_SEEK);

    // 检查元数据流、索引流的可读性与可定位性
    CHECK_CAPS(stream_m, CAPABILITY_READ_SEEK);
    CHECK_CAPS(stream_i, CAPABILITY_READ_SEEK);

/*
    // 读取各部分文件的头信息
    $header = self::_readHeaderWithCheck($stream_c, WPDP_Struct::HEADER_TYPE_CONTENTS);
    $header_m = self::_readHeaderWithCheck($stream_m, WPDP_Struct::HEADER_TYPE_METADATA);
    $header_i = self::_readHeaderWithCheck($stream_i, WPDP_Struct::HEADER_TYPE_INDEXES);

    // 读取各部分文件的区域信息
    $section = self::_readSectionWithCheck($stream_c, $header, WPDP_Struct::SECTION_TYPE_CONTENTS);
    $section_m = self::_readSectionWithCheck($stream_m, $header_m, WPDP_Struct::SECTION_TYPE_METADATA);
    $section_i = self::_readSectionWithCheck($stream_i, $header_i, WPDP_Struct::SECTION_TYPE_INDEXES);

    // 填充内容部分长度到基本块大小的整数倍
    $stream_c->seek(0, WPIO::SEEK_END);
    $padding = WPDP_Struct::BASE_BLOCK_SIZE - ($stream_c->tell() % WPDP_Struct::BASE_BLOCK_SIZE);
    $len_written = $stream_c->write(str_repeat("\x00", $padding));
    WPDP_StreamOperationException::checkIsWriteExactly($len_written, $padding);

    // 追加条目元数据
    header->ofsMetadata = wpio_tell(stream_c);
    self::_streamCopy($stream_c, $stream_m, $header_m['ofsMetadata'], $section_m['length']);

    // 追加条目索引
    header->ofsIndexes = wpio_tell(stream_c);
    self::_streamCopy($stream_c, $stream_i, $header_i['ofsIndexes'], $section_i['length']);

    // 更改文件类型为复合型
    header->type = FILE_TYPE_COMPOUND;

    // 更新头信息
    wpio_seek(stream_c, 0, SEEK_SET);
    $data_header = WPDP_Struct::packHeader($header);
    $len_written = $stream_c->write($data_header);
    WPDP_StreamOperationException::checkIsWriteExactly($len_written, strlen($data_header));
*/

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 导出数据堆
 *
 * @param stream_out   输出所要写入的流
 * @param type         导出文件类型
 */
WPDP_API int wpdp_export_stream(WPDP *dp, WPIO_Stream *stream_out, WPDP_ExportType type) {
    assert(IN_ARRAY_1(type, WPDP_EXPORT_LOOKUP));

//    StructHeader *header, *header_m, *header_i;
//    StructSection *section_m, *section_i;

    if (type != WPDP_EXPORT_LOOKUP) {
//                    "Invalid export type: %d", type);
        return WPDP_ERROR_INVALID_ARGUMENT;
    }

    // 检查输出流的可读性、可写性与可定位性
    CHECK_CAPS(stream_out, CAPABILITY_READ_WRITE_SEEK);

/*
    $stream_m = $this->_metadata->getStream();
    $stream_i = $this->_indexes->getStream();

    // 读取各部分文件的头信息
    $header_m = self::_readHeaderWithCheck($stream_m, WPDP_Struct::HEADER_TYPE_METADATA);
    $header_i = self::_readHeaderWithCheck($stream_i, WPDP_Struct::HEADER_TYPE_INDEXES);

    // 读取各部分文件的区域信息
    $section_m = self::_readSectionWithCheck($stream_m, $header_m, WPDP_Struct::SECTION_TYPE_METADATA);
    $section_i = self::_readSectionWithCheck($stream_i, $header_i, WPDP_Struct::SECTION_TYPE_INDEXES);

    // 复制一份元数据文件的头信息暂时作为查找文件的头信息
    $header = $header_m;
    $header['type'] = WPDP_Struct::HEADER_TYPE_UNDEFINED;
    // 将头信息写入到输出文件中
    $stream_out->seek(0, WPIO::SEEK_SET);
    $data_header = WPDP_Struct::packHeader($header);
    $len_written = $stream_out->write($data_header);
    WPDP_StreamOperationException::checkIsWriteExactly($len_written, strlen($data_header));

    // 写入条目元数据
    header->ofsMetadata = wpio_tell(stream_out);
    self::_streamCopy($stream_out, $stream_m, $header_m['ofsMetadata'], $section_m['length']);

    // 写入条目索引
    header->ofsIndexes = wpio_tell(stream_out);
    self::_streamCopy($stream_out, $stream_i, $header_i['ofsIndexes'], $section_i['length']);

    // 更改文件类型为查找型
    header->type = FILE_TYPE_LOOKUP;

    // 更新头信息
    $stream_out->seek(0, WPIO::SEEK_SET);
    $data_header = WPDP_Struct::packHeader($header);
    $len_written = $stream_out->write($data_header);
    WPDP_StreamOperationException::checkIsWriteExactly($len_written, strlen($data_header));
*/

    return WPDP_OK;
}

#endif

/**
 * 关闭当前打开的数据堆
 */
WPDP_API int wpdp_close(WPDP *dp) {
    if (!dp->_opened) {
//                    "The data pile has already closed");
        return WPDP_ERROR_BAD_FUNCTION_CALL;
    }

#ifndef BUILD_READONLY
    if (dp->_open_mode != WPDP_MODE_READONLY) {
        int rc = wpdp_flush(dp);
        RETURN_VAL_IF_NON_ZERO(rc);
    }
#endif

    dp->_contents = NULL;
    dp->_metadata = NULL;
    dp->_indexes = NULL;

    dp->_open_mode = 0;
#ifndef BUILD_READONLY
    dp->_cache_mode = 0;
    dp->_compression = 0;
    dp->_checksum = 0;
//    dp->attribute_indexes = 0;
#endif

    dp->_space_available = 0;

    dp->_opened = false;

    return WPDP_OK;
}

#ifndef BUILD_READONLY

/**
 * 将缓冲内容写入数据堆
 */
WPDP_API int wpdp_flush(WPDP *dp) {
//    CHECK_IS_WRITABLE_MODE(dp);

    section_contents_flush(dp->_contents);
    section_metadata_flush(dp->_metadata);
//    section_indexes_flush(dp->_indexes);

    return WPDP_OK;
}

#endif

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

#ifndef BUILD_READONLY

/**
 * 设置缓存模式
 *
 * @param mode 缓存模式
 */
WPDP_API int wpdp_set_cache_mode(WPDP *dp, WPDP_CacheMode mode) {
    assert(IN_ARRAY_2(mode, WPDP_CACHE_DISABLED, WPDP_CACHE_ENABLED));

//    CHECK_IS_WRITABLE_MODE(dp);

    switch (mode) {
        case WPDP_CACHE_DISABLED:
            dp->_cache_mode = WPDP_CACHE_DISABLED;
            break;
        case WPDP_CACHE_ENABLED:
            dp->_cache_mode = WPDP_CACHE_ENABLED;
            break;
        default:
//                        "Invalid cache mode: %d", mode);
            return WPDP_ERROR_INVALID_ARGUMENT;
            break;
    }

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 设置压缩类型
 *
 * @param type 压缩类型
 */
WPDP_API int wpdp_set_compression(WPDP *dp, WPDP_CompressionType type) {
    assert(IN_ARRAY_3(type, WPDP_COMPRESSION_NONE, WPDP_COMPRESSION_GZIP, WPDP_COMPRESSION_BZIP2));

//    CHECK_IS_WRITABLE_MODE(dp);

    switch (type) {
        case WPDP_COMPRESSION_NONE:
            dp->_compression = CONTENTS_COMPRESSION_NONE;
            break;
        case WPDP_COMPRESSION_GZIP:
            dp->_compression = CONTENTS_COMPRESSION_GZIP;
            break;
        case WPDP_COMPRESSION_BZIP2:
            dp->_compression = CONTENTS_COMPRESSION_BZIP2;
            break;
        default:
//                        "Invalid compression type: %d", type);
            return WPDP_ERROR_INVALID_ARGUMENT;
            break;
    }

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 设置校验类型
 *
 * @param type 校验类型
 */
WPDP_API int wpdp_set_checksum(WPDP *dp, WPDP_ChecksumType type) {
    assert(IN_ARRAY_4(type, WPDP_CHECKSUM_NONE, WPDP_CHECKSUM_CRC32, WPDP_CHECKSUM_MD5, WPDP_CHECKSUM_SHA1));

//    CHECK_IS_WRITABLE_MODE(dp);

    switch (type) {
        case WPDP_CHECKSUM_NONE:
            dp->_checksum = CONTENTS_CHECKSUM_NONE;
            break;
        case WPDP_CHECKSUM_CRC32:
            dp->_checksum = CONTENTS_CHECKSUM_CRC32;
            break;
        case WPDP_CHECKSUM_MD5:
            dp->_checksum = CONTENTS_CHECKSUM_MD5;
            break;
        case WPDP_CHECKSUM_SHA1:
            dp->_checksum = CONTENTS_CHECKSUM_SHA1;
            break;
        default:
//                        "Invalid checksum type: %d", type);
            return WPDP_ERROR_INVALID_ARGUMENT;
            break;
    }

    return WPDP_OK;
}

#endif

/**
 * 获取条目迭代器
 *
 * @return WPDP_Iterator 对象
 */
WPDP_API int wpdp_iterator_init(WPDP *dp, WPDP_Iterator **iterator_out) {
    PacketMetadata *meta_first;
    section_metadata_get_first(dp->_metadata, &meta_first);

    WPDP_Iterator *iterator = wpdp_new_zero(WPDP_Iterator, 1);
    iterator->dp = dp;
    iterator->first = meta_first;
    iterator->current = meta_first;

    *iterator_out = iterator;

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

#ifndef BUILD_READONLY

/**
 * 添加一个条目
 *
 * @param contents      条目内容
 * @param attributes    条目属性
 */
WPDP_API int wpdp_add(WPDP *dp, const char *contents, int64_t length,
                      WPDP_Entry_Attributes *attributes) {
    int rc;

    rc = wpdp_begin(dp, attributes, length);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = wpdp_transfer(dp, contents, length);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = wpdp_commit(dp);
    RETURN_VAL_IF_NON_ZERO(rc);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 开始一个数据传输
 *
 * @param attributes   条目属性
 * @param length       内容长度
 */
WPDP_API int wpdp_begin(WPDP *dp, WPDP_Entry_Attributes *attributes, int64_t length) {
    dp->_space_available = wpdp_file_space_available(dp);

    if (length > dp->_space_available) {
        return WPDP_ERROR_EXCEED_LIMIT;
    }

    dp->_args = wpdp_new_zero(WPDP_Entry_Args, 1);

    dp->_args->attributes = attributes;
    dp->_args->entry_info.compression = dp->_compression;
    dp->_args->entry_info.checksum = dp->_checksum;

    section_contents_begin(dp->_contents, length, dp->_args);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 传输数据
 *
 * @param data  数据
 */
WPDP_API int wpdp_transfer(WPDP *dp, const void *data, int32_t len) {
    if (len > dp->_space_available) {
        return WPDP_ERROR_EXCEED_LIMIT;
    }

    section_contents_transfer(dp->_contents, dp->_args, data, len);

    dp->_space_available -= len;

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 提交所传输数据
 *
 * @return 参数
 */
WPDP_API int wpdp_commit(WPDP *dp) {
    section_contents_commit(dp->_contents, dp->_args);
    section_metadata_add(dp->_metadata, dp->_args);
    section_indexes_index(dp->_indexes, dp->_args);

    dp->_space_available = 0;
//    unset($this->_space_available);
    wpdp_free(dp->_args);

    if (dp->_cache_mode == WPDP_CACHE_DISABLED) {
        wpdp_flush(dp);
    }

    return WPDP_OK;
}

#endif

/**
 * 获取当前数据堆文件的版本
 *
 * @return 当前数据堆文件的版本
 */
WPDP_API void *wpdp_file_info(WPDP *dp) {
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

#ifdef BUILD_READONLY
    if (mode != WPDP_MODE_READONLY) {
        error_set_msg("This is a readonly build of WPDP");
        return WPDP_ERROR_INVALID_ARGUMENT;
    }
#endif

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
