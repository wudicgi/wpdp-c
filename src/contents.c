#include "internal.h"
#include <math.h>

#define BUFFER_SIZE     (512 * 1024)    // 512KB
#define BUILDER_SIZE    1024

typedef struct _SectionContentsCustom   Custom;

// Contents.php: class WPDP_Contents extends WPDP_Common
struct _SectionContentsCustom {
    char                    _buffer[BUFFER_SIZE];
    int32_t                 _buffer_pos;
};

static int _get_offsets_and_sizes(Section *sect, WPDP_Entry_Args *args,
                                  int64_t **offsets_out, int32_t **sizes_out);

int section_contents_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    int rc = section_init(SECTION_TYPE_CONTENTS, stream, mode, sect_out);
    RETURN_VAL_IF_NON_ZERO(rc);

    (*sect_out)->custom = wpdp_new_zero(Custom, 1);

    return WPDP_OK;
}

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
