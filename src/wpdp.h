#ifndef _WPDP_H_
#define _WPDP_H_

#ifndef WPDP_API
# define WPDP_API __declspec(dllexport)
#endif

typedef enum _WPDP_OpenMode         WPDP_OpenMode;
typedef enum _WPDP_CacheMode        WPDP_CacheMode;
typedef enum _WPDP_CompressionType  WPDP_CompressionType;
typedef enum _WPDP_ChecksumType     WPDP_ChecksumType;
typedef enum _WPDP_ExportType       WPDP_ExportType;

typedef struct _WPDP                WPDP;

typedef struct _WPDP_Entry          WPDP_Entry;
typedef struct _WPDP_Entry_Info     WPDP_Entry_Info;

typedef struct _WPDP_Entry_Attributes   WPDP_Entry_Attributes;
typedef struct _WPDP_Entry_Attribute    WPDP_Entry_Attribute;

typedef struct _WPDP_String_Builder     WPDP_String_Builder;
typedef struct _WPDP_String             WPDP_String;

typedef struct _WPDP_Iterator       WPDP_Iterator;
typedef struct _WPDP_Entries        WPDP_Entries;

/**
 * 打开模式常量
 */
enum _WPDP_OpenMode {
    WPDP_MODE_READONLY = 1,     // 只读方式
    WPDP_MODE_READWRITE = 2     // 读写方式
};

/**
 * 缓存方式常量
 */
enum _WPDP_CacheMode {
    WPDP_CACHE_DISABLED = 0,    // 禁用所有缓存
    WPDP_CACHE_ENABLED = 1      // 启用所有缓存
};

/**
 * 压缩类型常量
 */
enum _WPDP_CompressionType {
    WPDP_COMPRESSION_NONE = 0,  // 不压缩
    WPDP_COMPRESSION_GZIP = 1,  // Gzip
    WPDP_COMPRESSION_BZIP2 = 2  // Bzip2
};

/**
 * 校验类型常量
 */
enum _WPDP_ChecksumType {
    WPDP_CHECKSUM_NONE = 0,     // 不校验
    WPDP_CHECKSUM_CRC32 = 1,    // CRC32
    WPDP_CHECKSUM_MD5 = 2,      // MD5
    WPDP_CHECKSUM_SHA1 = 3      // SHA1
};

/**
 * 导出类型常量
 */
enum _WPDP_ExportType {
    WPDP_EXPORT_LOOKUP = 0x06   // 用于查找条目的文件
};

// WPDP.php: class WPDP
struct _WPDP {
    // 各区域的操作对象
    Section             *_contents;
    Section             *_metadata;
    Section             *_indexes;
    // 当前数据堆的操作参数
    WPDP_OpenMode       _open_mode;
    WPDP_CacheMode      _cache_mode;
#ifndef BUILD_READONLY
    uint8_t             _compression;
#endif
#ifndef BUILD_READONLY
    uint8_t             _checksum;
#endif
#ifndef BUILD_READONLY
    WPDP_String         **_attribute_indexes;
#endif
    WPDP_Entry_Args     *_args;
    // 当前数据堆的文件信息
    uint16_t            _file_version;
    uint8_t             _file_type;
    uint8_t             _file_limit;
    // 当前数据堆的操作信息
    bool                _opened;
    int64_t             _space_available;
    // Add
    WPIO_Stream         *_stream_c;
    WPIO_Stream         *_stream_m;
    WPIO_Stream         *_stream_i;
};

struct _WPDP_Entry {
    WPDP        *dp;
    void        *metadata;
    void        *attributes;
};

// Entry.php: class WPDP_Entry_Information
struct _WPDP_Entry_Info {
    uint8_t     compression;
    uint8_t     checksum;
    int32_t     chunk_size;
    int32_t     chunk_count;
    int64_t     original_length;
    int64_t     compressed_length;
};

struct _WPDP_Entry_Attributes {
    int                     capacity;
    int                     size;
    WPDP_Entry_Attribute    **attrs;
};

struct _WPDP_Entry_Attribute {
    WPDP_String     *name;
    WPDP_String     *value;
    bool            index;
};

struct _WPDP_String_Builder {
    void    *str;
    int     length;
    int     capacity;
};

struct _WPDP_String {
    void    *str;
    int     len;
};

struct _WPDP_Iterator {
    WPDP            *dp;
    PacketMetadata  *first;
    PacketMetadata  *current;
};

struct _WPDP_Entries {
    WPDP        *dp;
//    GArray      *offsets;
    int         position;
};


WPDP_API int wpdp_open_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                WPIO_Stream *stream_i, WPDP_OpenMode mode, WPDP **wpdp);
#ifndef BUILD_READONLY
WPDP_API int wpdp_create_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                     WPIO_Stream *stream_i);
WPDP_API int wpdp_compound_stream(WPIO_Stream *stream_c, WPIO_Stream *stream_m,
                                       WPIO_Stream *stream_i);
#endif
WPDP_API int wpdp_export_stream(WPDP *dp, WPIO_Stream *stream_out, WPDP_ExportType type);

/*
WPDP_API WPDP *wpdp_open(const char *filename, WPDP_OpenMode mode);
WPDP_API int wpdp_create(const char *filename);

WPDP_API int wpdp_compound(const char *filename);
WPDP_API int wpdp_export(WPDP *dp, const char *filename_out, WPDP_ExportType type);
*/
WPDP_API int wpdp_close(WPDP *dp);
#ifndef BUILD_READONLY
WPDP_API int wpdp_flush(WPDP *dp);
#endif

WPDP_API int64_t wpdp_file_space_used(WPDP *dp);
WPDP_API int64_t wpdp_file_space_available(WPDP *dp);

#ifndef BUILD_READONLY
WPDP_API int wpdp_set_cache_mode(WPDP *dp, WPDP_CacheMode mode);
WPDP_API int wpdp_set_compression(WPDP *dp, WPDP_CompressionType type);
WPDP_API int wpdp_set_checksum(WPDP *dp, WPDP_ChecksumType type);
#endif

WPDP_API int wpdp_iterator_init(WPDP *dp, WPDP_Iterator **iterator_out);
WPDP_API int wpdp_iterator_next(WPDP_Iterator *iterator);

WPDP_API void *wpdp_query(WPDP *dp, const char *attr_name, const char *attr_value);

#ifndef BUILD_READONLY
WPDP_API int wpdp_add(WPDP *dp, const char *contents, int64_t length,
                      WPDP_Entry_Attributes *attributes);
WPDP_API int wpdp_begin(WPDP *dp, WPDP_Entry_Attributes *attributes, int64_t length);
WPDP_API int wpdp_transfer(WPDP *dp, const void *data, int32_t len);
WPDP_API int wpdp_commit(WPDP *dp);
#endif

WPDP_API void *wpdp_file_info(WPDP *dp);
WPDP_API char *wpdp_library_version(void);

WPDP_String_Builder *wpdp_string_builder_create(int init_capacity);
int wpdp_string_builder_append(WPDP_String_Builder *builder, void *data, int len);
int wpdp_string_builder_free(WPDP_String_Builder *builder);

WPDP_String *wpdp_string_direct(void *str, int len);
WPDP_String *wpdp_string_create(const char *str, int len);
WPDP_String *wpdp_string_from_cstr(const char *str);
int wpdp_string_compare(WPDP_String *str_1, WPDP_String *str_2);
int wpdp_string_free(WPDP_String *str);

#endif // _WPDP_H_
