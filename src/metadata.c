#include "internal.h"

#ifndef BUILD_READONLY
static int _write_metadata(Section *sect, StructMetadata *metadata, int64_t *offset_out);
#endif

int section_metadata_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    int rc = section_init(SECTION_TYPE_METADATA, stream, mode, sect_out);
    RETURN_VAL_IF_NON_ZERO(rc);

    (*sect_out)->custom = NULL;

    return WPDP_OK;
}

#ifndef BUILD_READONLY

int section_metadata_create(WPIO_Stream *stream) {
    section_create(HEADER_TYPE_METADATA, SECTION_TYPE_METADATA, stream);

    // 写入了重要的结构和信息，将流的缓冲区写入
    wpio_flush(stream);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

/**
 * 将缓冲内容写入文件
 */
int section_metadata_flush(Section *sect) {
    section_seek(sect, 0, SEEK_END, _ABSOLUTE);
    int64_t length = section_tell(sect, _RELATIVE);
    sect->_section->length = length;
    section_write_section(sect);

    wpio_flush(sect->_stream);

    return WPDP_OK;
}

#endif

int64_t section_metadata_get_section_length(Section *sect) {
    return sect->_section->length;
}

int section_metadata_get_metadata(Section *sect, int64_t offset, PacketMetadata **p_metadata_out) {
    PacketMetadata *p_metadata;

    p_metadata = wpdp_new_zero(PacketMetadata, 1);

    section_seek(sect, offset, SEEK_SET, _RELATIVE);

    struct_read_metadata(sect->_stream, &p_metadata->metadata, false);
    p_metadata->offset = offset;

    *p_metadata_out = p_metadata;

    return WPDP_OK;
}

int section_metadata_get_first(Section *sect, PacketMetadata **p_metadata_out) {
    if (sect->_section->ofsFirst == 0) {
        return WPDP_ERROR;
    }

    return section_metadata_get_metadata(sect, sect->_section->ofsFirst, p_metadata_out);
}

int section_metadata_get_next(Section *sect, PacketMetadata *p_current, PacketMetadata **p_next_out) {
    int64_t offset_next = p_current->offset + p_current->metadata->lenBlock;
    if (offset_next >= sect->_section->length) {
        return WPDP_ERROR;
    }

    return section_metadata_get_metadata(sect, offset_next, p_next_out);
}

#ifndef BUILD_READONLY

int section_metadata_add(Section *sect, WPDP_Entry_Args *args) {
    StructMetadata *metadata;
    int i;

    struct_create_metadata(&metadata);

    metadata->compression = args->entry_info.compression;
    metadata->checksum = args->entry_info.checksum;
    metadata->sizeChunk = args->entry_info.chunk_size;
    metadata->numChunk = args->entry_info.chunk_count;
    metadata->lenOriginal = args->entry_info.original_length;
    metadata->lenCompressed = args->entry_info.compressed_length;
    metadata->ofsContents = args->contents_offset;
    metadata->ofsOffsetTable = args->offset_table_offset;
    metadata->ofsChecksumTable = args->checksum_table_offset;

    WPDP_Entry_Attributes *attrs = args->attributes;
    WPDP_String_Builder *builder = wpdp_string_builder_create(256);
    uint8_t signature = ATTRIBUTE_SIGNATURE;
    for (i = 0; i < attrs->size; i++) {
        WPDP_Entry_Attribute *attr = attrs->attrs[i];
        uint8_t flag = ATTRIBUTE_FLAG_NONE;
        if (attr->index) {
            flag |= ATTRIBUTE_FLAG_INDEXED;
        }
        uint8_t attr_name_len = attr->name->len;
        uint16_t attr_value_len = attr->value->len;
        wpdp_string_builder_append(builder, &signature, sizeof(signature));
        wpdp_string_builder_append(builder, &flag, sizeof(flag));
        wpdp_string_builder_append(builder, &attr_name_len, sizeof(attr_name_len));
        wpdp_string_builder_append(builder, attr->name->str, attr->name->len);
        wpdp_string_builder_append(builder, &attr_value_len, sizeof(attr_value_len));
        wpdp_string_builder_append(builder, attr->value->str, attr->value->len);
    }
/*
    char *debug = malloc(builder->length + 1);
    memcpy(debug, builder->str, builder->length);
    debug[builder->length] = '\0';
    printf("%s\r\n", debug);
*/
    metadata->lenActual = (int)sizeof(StructMetadata) + builder->length;
    metadata->lenBlock = struct_get_block_length(METADATA_BLOCK_SIZE, metadata->lenActual);

    metadata = wpdp_realloc(metadata, metadata->lenBlock);

    memcpy(metadata->blob, builder->str, (size_t)builder->length);

    wpdp_string_builder_free(builder);

    // 写入该元数据
    int64_t metadata_offset;
    _write_metadata(sect, metadata, &metadata_offset);

    args->metadata_offset = metadata_offset;

    if (sect->_section->ofsFirst == 0) {
        sect->_section->ofsFirst = metadata_offset;
        section_write_section(sect);
    }

    // 写入了重要的信息，将流的缓冲区写入
    section_metadata_flush(sect);

    return WPDP_OK;
}

#endif

#ifndef BUILD_READONLY

static int _write_metadata(Section *sect, StructMetadata *metadata, int64_t *offset_out) {
    int64_t offset;

    section_seek(sect, 0, SEEK_END, _ABSOLUTE); // to be noticed
    offset = section_tell(sect, _RELATIVE);

    struct_write_metadata(sect->_stream, metadata);

    sect->_section->length = section_tell(sect, _RELATIVE);

    *offset_out = offset;

    return WPDP_OK;
}

#endif
