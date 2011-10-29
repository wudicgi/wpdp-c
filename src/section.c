#include "internal.h"

#define ABSOLUTE_OFFSET(sect, offset) ((sect)->_offset_base + (offset))
#define RELATIVE_OFFSET(sect, offset) ((offset) - (sect)->_offset_base)

static int64_t _get_section_offset(Section *sect, uint8_t sect_type);

/**
 * 构造函数
 *
 * @param type     区域类型
 * @param stream   文件操作对象
 * @param mode     打开模式
 */
int section_init(uint8_t sect_type, WPIO_Stream *stream,
                 WPDP_OpenMode mode, Section **sect_out) {
    assert(IN_ARRAY_3(sect_type, SECTION_TYPE_CONTENTS, SECTION_TYPE_METADATA, SECTION_TYPE_INDEXES));
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    Section *sect;

    sect = wpdp_new_zero(Section, 1);
    if (sect == NULL) {
        return WPDP_ERROR;
    }

    sect->_open_mode = mode;
    sect->_stream = stream;
    sect->type = sect_type;

    section_read_header(sect);
    section_read_section(sect, sect_type);

    *sect_out = sect;

    return WPDP_OK;
}

WPIO_Stream *section_get_stream(Section *sect) {
    return sect->_stream;
}

int section_read_header(Section *sect) {
    section_seek(sect, 0, SEEK_SET, _ABSOLUTE);
    struct_read_header(sect->_stream, &sect->_header);

    return WPDP_OK;
}

int section_read_section(Section *sect, uint8_t sect_type) {
    assert(IN_ARRAY_3(sect_type, SECTION_TYPE_CONTENTS, SECTION_TYPE_METADATA, SECTION_TYPE_INDEXES));

    int rc;
    int64_t offset;

    offset = _get_section_offset(sect, sect_type);
    section_seek(sect, offset, SEEK_SET, _ABSOLUTE);
    rc = struct_read_section(sect->_stream, &sect->_section);
    RETURN_VAL_IF_NON_ZERO(rc);
    sect->_offset_base = offset;

    return WPDP_OK;
}

/**
 * 获取当前位置的偏移量
 *
 * @param offset_type  偏移量类型 (可选，默认为绝对偏移量)
 *
 * @return 偏移量
 */
int64_t section_tell(Section *sect, OffsetType offset_type) {
    assert(IN_ARRAY_2(offset_type, _ABSOLUTE, _RELATIVE));

    int64_t offset = wpio_tell(sect->_stream);

    if (offset_type == _RELATIVE) {
        offset = RELATIVE_OFFSET(sect, offset);
    }

    return offset;
}

/**
 * 定位到指定偏移量
 *
 * @param offset       偏移量
 * @param origin       whence (可选，默认为 WPIO::SEEK_SET)
 * @param offset_type  偏移量类型 (可选，默认为绝对偏移量)
 *
 * @return 总是 true
 */
int section_seek(Section *sect, int64_t offset, int origin, OffsetType offset_type) {
    assert(IN_ARRAY_3(origin, SEEK_SET, SEEK_CUR, SEEK_END));
    assert(IN_ARRAY_2(offset_type, _ABSOLUTE, _RELATIVE));

    if (offset_type == _RELATIVE) {
        origin = SEEK_SET;
        offset = ABSOLUTE_OFFSET(sect, offset);
    }

    wpio_seek(sect->_stream, offset, origin);

    return WPDP_OK;
}

/**
 * 从当前位置开始读取指定长度的数据
 *
 * @param length       要读取数据的长度
 * @param offset       偏移量 (可选，默认为 null，即从当前位置开始读取)
 * @param offset_type  偏移量类型 (可选，默认为绝对偏移量)
 *
 * @return 读取到的数据
 */
int section_read(Section *sect, void *buffer, int length) {
    int len_didread;

    len_didread = (int)wpio_read(sect->_stream, buffer, (size_t)length);
//    WPDP_StreamOperationException::checkIsReadExactly(strlen($data), $length);

    return WPDP_OK;
}

/**
 * 获取区域的绝对偏移量
 *
 * @param type 区域类型
 *
 * @return 区域的绝对偏移量
 */
static int64_t _get_section_offset(Section *sect, uint8_t sect_type) {
    assert(IN_ARRAY_3(sect_type, SECTION_TYPE_CONTENTS, SECTION_TYPE_METADATA, SECTION_TYPE_INDEXES));

    switch (sect_type) {
        case SECTION_TYPE_CONTENTS:
            return sect->_header->ofsContents;
            break;
        case SECTION_TYPE_METADATA:
            return sect->_header->ofsMetadata;
            break;
        case SECTION_TYPE_INDEXES:
            return sect->_header->ofsIndexes;
            break;
        default:
            return 0; // to be noticed
            break;
    }
}
