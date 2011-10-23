#include "internal.h"
#include <math.h>

#define BUFFER_SIZE     (512 * 1024)    // 512KB
#define BUILDER_SIZE    1024

typedef struct _SectionContentsCustom   Custom;

// Contents.php: class WPDP_Contents extends WPDP_Common
struct _SectionContentsCustom {
    char                    _buffer[BUFFER_SIZE];
    int32_t                 _buffer_pos;
#ifndef BUILD_READONLY
    int64_t                 _bytes_written;
    WPDP_String_Builder     *_chunk_offsets;
    WPDP_String_Builder     *_chunk_checksums;
#endif
};

#ifndef BUILD_READONLY
static int _write_buffer(Section *sect, WPDP_Entry_Args *args);
#endif

static int _get_offsets_and_sizes(Section *sect, WPDP_Entry_Args *args,
                                  int64_t **offsets_out, int32_t **sizes_out);

#ifndef BUILD_READONLY
static int32_t _compute_chunk_size(int64_t length);
static int32_t _min_length(int32_t val1, int32_t val2);
#endif

int section_contents_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    int rc = section_init(SECTION_TYPE_CONTENTS, stream, mode, sect_out);
    RETURN_VAL_IF_NON_ZERO(rc);

    (*sect_out)->custom = wpdp_new_zero(Custom, 1);

    return WPDP_OK;
}

#ifndef BUILD_READONLY

int section_contents_create(WPIO_Stream *stream) {
    section_create(HEADER_TYPE_CONTENTS, SECTION_TYPE_CONTENTS, stream);

    // 写入了重要的结构和信息，将流的缓冲区写入
    wpio_flush(stream);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 将缓冲内容写入文件
 */
int section_contents_flush(Section *sect) {
    section_seek(sect, 0, SEEK_END, _ABSOLUTE);
    int64_t length = section_tell(sect, _RELATIVE);
    sect->_section->length = length;
    section_write_section(sect);

    wpio_flush(sect->_stream);

    return WPDP_OK;
}

#endif

int64_t section_contents_get_section_length(Section *sect) {
    return sect->_section->length;
}


#define min(a, b)   (((a) < (b)) ? (a) : (b))


int section_contents_get_contents(Section *sect, WPDP_Entry_Args *args, int64_t offset, int64_t length,
                                  void *data, int64_t *length_read) {
    Custom *custom = (Custom*)sect->custom;

    trace("offset = 0x%llX, file length = %lld, length to read = %lld", offset,
          args->entry_info.original_length, length);

    if (offset < 0) {
        error_set_msg("The offset parameter cannot be negative");
        return WPDP_ERROR_INTERNAL;
    }

    if (offset > args->entry_info.original_length) {
        error_set_msg("The offset parameter exceeds EOF");
        return WPDP_ERROR_INTERNAL;
    }

    if (length < 0) {
        error_set_msg("The length parameter cannot be negative");
        return WPDP_ERROR_INTERNAL;
    }

    if (offset + length > args->entry_info.original_length) {
        length = args->entry_info.original_length - offset;
    }

    trace("offset = 0x%llX, file length = %lld, length to read = %lld", offset,
          args->entry_info.original_length, length);

    int64_t *offsets;
    int32_t *sizes;

    _get_offsets_and_sizes(sect, args, &offsets, &sizes);

//    void *data = wpdp_malloc_zero(length);
    int64_t didread = 0;

    char buffer[BUFFER_SIZE];

    while (didread < length) {
        int chunk_index = (int)(offset / args->entry_info.chunk_size);

        section_seek(sect, args->contents_offset + offsets[chunk_index], SEEK_SET, _ABSOLUTE);
        int buf_read_len = section_read(sect, buffer, sizes[chunk_index]);

        if (args->entry_info.compression != CONTENTS_COMPRESSION_NONE) {
            // _decompress($chunk, $args->compression);
        }

        int len_ahead = offset % args->entry_info.chunk_size;
        int len_behind = buf_read_len - len_ahead;
        int len_read = min(len_behind, length - didread);
        trace("len_ahead = %d, len_behind = %d, len_read = %d", len_ahead, len_behind, len_read);
        if (len_ahead == 0 && len_read == args->entry_info.chunk_size) {
            memcpy(data + didread, buffer, BUFFER_SIZE);
        } else {
            memcpy(data + didread, buffer + len_ahead, (size_t)len_read);
        }
        didread += len_read;
        offset += len_read;
        trace("len_read = %d, current offset = 0x%llX", len_read, offset);
    }

    length_read = didread;

    trace("length_didread = %d, strlen(data) = %d, offset = 0x%llX", didread, strlen($data), offset);

//    return data;
    return WPDP_OK;
}





static int _get_offsets_and_sizes(Section *sect, WPDP_Entry_Args *args,
                                  int64_t **offsets_out, int32_t **sizes_out) {
    int i;

    if (args->offset_table_offset != 0) {
        section_seek(sect, args->offset_table_offset, SEEK_SET, _ABSOLUTE);

        int64_t *offsets = wpdp_malloc_zero(args->entry_info.chunk_count * 8);
        section_read(sect, (void *)offsets, args->entry_info.chunk_count * 8);

        int32_t *sizes = (int32_t *)wpdp_malloc_zero(args->entry_info.chunk_size * (int)sizeof(int32_t));

        int64_t offset_temp = args->entry_info.compressed_length;
        for (i = args->entry_info.chunk_count - 1; i >= 0; i--) {
            sizes[i] = offset_temp - offsets[i];
            offset_temp = offsets[i];
        }

        *offsets_out = offsets;
        *sizes_out = sizes;
    } else if (args->entry_info.compression == CONTENTS_COMPRESSION_NONE) {
        int64_t *offsets = wpdp_malloc_zero(args->entry_info.chunk_count * 8);
        int32_t *sizes = (int32_t *)wpdp_malloc_zero(args->entry_info.chunk_size * (int)sizeof(int32_t));

        for (i = 0; i < args->entry_info.chunk_count; i++) {
            offsets[i] = args->entry_info.chunk_size * i;
            sizes[i] = args->entry_info.chunk_size;
        }
        sizes[args->entry_info.chunk_count - 1] = args->entry_info.compressed_length
            - offsets[args->entry_info.chunk_count - 1];

        *offsets_out = offsets;
        *sizes_out = sizes;
    } else {
        return WPDP_ERROR_FILE_BROKEN;
    }

    return WPDP_OK;
}




#ifndef BUILD_READONLY

int section_contents_begin(Section *sect, int64_t length, WPDP_Entry_Args *args) {
    Custom *custom = (Custom*)sect->custom;

    section_seek(sect, 0, SEEK_END, _ABSOLUTE);
    int64_t offset_begin = section_tell(sect, _ABSOLUTE);

    args->contents_offset = offset_begin;
    args->offset_table_offset = 0;
    args->checksum_table_offset = 0;

    // $args->compression, $args->checksum 在调用本方法前设置

    args->entry_info.chunk_size = _compute_chunk_size(length);
    args->entry_info.chunk_count = 0;
    args->entry_info.original_length = 0;
    args->entry_info.compressed_length = 0;

    memset(custom->_buffer, 0, sizeof(custom->_buffer));
    custom->_buffer_pos = 0;
    custom->_bytes_written = 0;

    custom->_chunk_offsets = wpdp_string_builder_create(BUILDER_SIZE);
    custom->_chunk_checksums = wpdp_string_builder_create(BUILDER_SIZE);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

int section_contents_transfer(Section *sect, WPDP_Entry_Args *args, const void *data, int32_t len) {
    Custom *custom = (Custom*)sect->custom;

    int32_t pos = 0;
    while (pos < len) {
        int32_t tmp = _min_length(len - pos, args->entry_info.chunk_size - custom->_buffer_pos);
        memcpy((custom->_buffer + custom->_buffer_pos), (data + pos), (size_t)tmp);
        custom->_buffer_pos += tmp;
        assert(custom->_buffer_pos <= args->entry_info.chunk_size);
        if (custom->_buffer_pos == args->entry_info.chunk_size) {
            // 已填满一个 chunk，写入缓冲区数据
            _write_buffer(sect, args);
        }
        pos += tmp;
    }

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

int section_contents_commit(Section *sect, WPDP_Entry_Args *args) {
    Custom *custom = (Custom*)sect->custom;

    // 写入缓冲区中剩余数据
    _write_buffer(sect, args);

    // 计算分块数量
    args->entry_info.chunk_count = custom->_chunk_offsets->length / (int)sizeof(custom->_bytes_written);

    // 若已启用压缩，写入分块偏移量表
    if (args->entry_info.compression != CONTENTS_COMPRESSION_NONE) { // WPDP_Struct::
        args->offset_table_offset = section_tell(sect, _ABSOLUTE);
        section_write(sect, custom->_chunk_offsets->str, custom->_chunk_offsets->length);
    }

    // 若已启用校验，写入分块校验值表
    if (args->entry_info.checksum != CONTENTS_CHECKSUM_NONE) { // WPDP_Struct::
        args->checksum_table_offset = section_tell(sect, _ABSOLUTE);
        section_write(sect, custom->_chunk_checksums->str, custom->_chunk_checksums->length);
    }

    sect->_section->length = section_tell(sect, _RELATIVE);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

static int _write_buffer(Section *sect, WPDP_Entry_Args *args) {
    Custom *custom = (Custom*)sect->custom;

    // 获取缓冲区中内容的实际长度
    int32_t len_actual = custom->_buffer_pos;
    // 若长度为 0，不进行任何操作
    if (len_actual == 0) {
        return WPDP_OK;
    }

    // 计算该块的校验值 (若已设置)
    switch (args->entry_info.checksum) {
        case CONTENTS_CHECKSUM_CRC32:
//            $this->_chunkChecksums[] = pack('V', crc32($this->_buffer));
            break;
        case CONTENTS_CHECKSUM_MD5:
//            $this->_chunkChecksums[] = md5($this->_buffer, true);
            break;
        case CONTENTS_CHECKSUM_SHA1:
//            $this->_chunkChecksums[] = sha1($this->_buffer, true);
            break;
    }

    // 压缩该块数据 (若已设置)
    if (args->entry_info.compression != CONTENTS_COMPRESSION_NONE) {
//        $this->_compress($this->_buffer, $args->compression);
    }
    int32_t len_compressed = custom->_buffer_pos; // strlen($this->_buffer);

    // 计算块的偏移量和结尾填充长度
    wpdp_string_builder_append(custom->_chunk_offsets, &custom->_bytes_written,
        sizeof(custom->_bytes_written));

    // 累加内容原始大小和压缩后大小
    args->entry_info.original_length += len_actual;
    args->entry_info.compressed_length += len_compressed;

    // 写入该块数据
    section_write(sect, custom->_buffer, custom->_buffer_pos);

    sect->_section->length = section_tell(sect, _RELATIVE);

    // 累加已写入字节数
    custom->_bytes_written += len_compressed;

    // 清空缓冲区
    memset(custom->_buffer, 0, sizeof(custom->_buffer));
    custom->_buffer_pos = 0;

    return WPDP_OK;
}

#endif

/*
static void _decompress(void *buffer, size_t length, int type) {
    assert('in_array($type, array(WPDP_Struct::CONTENTS_COMPRESSION_NONE, WPDP_Struct::CONTENTS_COMPRESSION_GZIP, WPDP_Struct::CONTENTS_COMPRESSION_BZIP2))');

    switch (type) {
        case CONTENTS_COMPRESSION_NONE:
            break;
        case CONTENTS_COMPRESSION_GZIP:
            $data = gzuncompress($data);
            break;
        case CONTENTS_COMPRESSION_BZIP2:
            $data = bzdecompress($data);
            break;
        default:
//            assert('false');
            break;
    }
}
*/

#ifndef BUILD_READONLY

/**
 * 计算分块大小
 *
 * 分块大小根据内容长度确定。当内容长度不足 64MB 时，分块大小为 16KB；
 * 超过 1024MB 时，分块大小为 512KB；介于 64MB 和 1024MB 之间时使用
 * chunk_size = 2 ^ ceil(log2(length / 4096)) 计算。
 *
 * @param length  内容长度
 *
 * @return 分块大小
 */
static int32_t _compute_chunk_size(int64_t length) {
    if (length <= (64 * 1024 * 1024)) { // 64MB
        return (16 * 1024); // 16KB
    } else if (length > (1024 * 1024 * 1024)) { // 1024MB
        return (512 * 1024); // 512KB
    } else {
        return (int32_t)pow(2, ceil(log2((double)length / 4096)));
    }
}

#endif

#ifndef BUILD_READONLY

static int32_t _min_length(int32_t val1, int32_t val2) {
    return (val1 < val2) ? val1 : val2;
}

#endif
