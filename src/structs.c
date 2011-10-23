#include "internal.h"
#include <math.h>

#define STRUCT_READ_FIXED(stream, ptr_out, struct_type)    do { \
        struct_type *ptr; \
        size_t len; \
        ptr = wpdp_new_zero(struct_type, 1); \
        len = wpio_read((stream), ptr, sizeof(struct_type)); \
        CHECK_IS_READ_EXACTLY(len, sizeof(struct_type), error); \
        *(ptr_out) = ptr; \
    } while (0)

#define STRUCT_WRITE_FIXED(stream, ptr, struct_type) \
    wpio_write(stream, ptr, sizeof(struct_type))

#define STRUCT_READ_VARIANT(stream, ptr_out, noblob, struct_type, struct_sign, block_size)    do { \
        struct_type *ptr = wpdp_malloc_zero((block_size)); \
        size_t len = wpio_read((stream), ptr, (size_t)(block_size)); \
        if (ptr->signature != (struct_sign)) { \
            error_set_msg("Unexpected signature 0x%X, expecting 0x%X", \
                          ptr->signature, (struct_sign)); \
            return RETURN_CODE(WPDP_ERROR_FILE_BROKEN); \
        } \
        if ((noblob)) { \
            *(ptr_out) = ptr; \
            return RETURN_CODE(WPDP_OK); \
        } \
        if (ptr->lenBlock > (block_size)) { \
            ptr = wpdp_realloc(ptr, ptr->lenBlock); \
            len = wpio_read((stream), (ptr + (block_size)), ((size_t)ptr->lenBlock - (size_t)(block_size))); \
        } \
        *(ptr_out) = ptr; \
    } while (0)

int struct_create_header(StructHeader **header_out) {
    StructHeader *header;

    header = wpdp_malloc_zero(HEADER_BLOCK_SIZE);
    if (header == NULL) {
        return RETURN_CODE(WPDP_ERROR);
    }

    header->signature      = HEADER_SIGNATURE;
    header->version        = HEADER_THIS_VERSION;
    header->flags          = HEADER_FLAG_NONE;
    header->type           = HEADER_TYPE_UNDEFINED;
    header->limit          = HEADER_LIMIT_INT32;
    header->__r_char_1     = 0;
    header->__r_char_2     = 0;
    header->ofsContents    = 0;
    header->ofsMetadata    = 0;
    header->ofsIndexes     = 0;

    *header_out = header;

    return RETURN_CODE(WPDP_OK);
}

int struct_create_section(StructSection **section_out) {
    StructSection *section;

    section = wpdp_malloc_zero(SECTION_BLOCK_SIZE);
    if (section == NULL) {
        return RETURN_CODE(WPDP_ERROR);
    }

    section->signature  = SECTION_SIGNATURE;
    section->type       = 0;
    section->__r_char   = 0;
    section->length     = 0;
    section->ofsTable   = 0;
    section->ofsFirst   = 0;

    *section_out = section;

    return RETURN_CODE(WPDP_OK);
}

int struct_create_metadata(StructMetadata **metadata_out) {
    StructMetadata *metadata;

    metadata = wpdp_malloc_zero(METADATA_BLOCK_SIZE);
    if (metadata == NULL) {
        return RETURN_CODE(WPDP_ERROR);
    }

    metadata->signature         = METADATA_SIGNATURE;
    metadata->lenBlock          = METADATA_BLOCK_SIZE;
    metadata->lenActual         = (int32_t)sizeof(StructMetadata);
    metadata->flags             = METADATA_FLAG_NONE;
    metadata->compression       = CONTENTS_COMPRESSION_NONE;
    metadata->checksum          = CONTENTS_CHECKSUM_NONE;
    metadata->lenOriginal       = 0;
    metadata->lenCompressed     = 0;
    metadata->sizeChunk         = 0;
    metadata->numChunk          = 0;
    metadata->ofsContents       = 0;
    metadata->ofsOffsetTable    = 0;
    metadata->ofsChecksumTable  = 0;

    *metadata_out = metadata;

    return RETURN_CODE(WPDP_OK);
}

int struct_create_index_table(StructIndexTable **index_table_out) {
    StructIndexTable *index_table;

    index_table = wpdp_malloc_zero(INDEX_TABLE_BLOCK_SIZE);
    if (index_table == NULL) {
        return RETURN_CODE(WPDP_ERROR);
    }

    index_table->signature      = INDEX_TABLE_SIGNATURE;
    index_table->lenBlock       = INDEX_TABLE_BLOCK_SIZE;
    index_table->lenActual      = (int32_t)sizeof(StructIndexTable);

    *index_table_out = index_table;

    return RETURN_CODE(WPDP_OK);
}

int struct_create_node(StructNode **node_out) {
    StructNode *node;

    node = wpdp_malloc_zero(NODE_BLOCK_SIZE);
    if (node == NULL) {
        return RETURN_CODE(WPDP_ERROR);
    }

    node->signature      = NODE_SIGNATURE;
    node->isLeaf         = 0;
    node->__r_char       = 0;
    node->numElement     = 0;
    node->ofsExtra       = 0;

    *node_out = node;

    return RETURN_CODE(WPDP_OK);
}

int struct_read_header(WPIO_Stream *stream, StructHeader **header_out) {
    STRUCT_READ_FIXED(stream, header_out, StructHeader);

    if ((*header_out)->signature != HEADER_SIGNATURE) {
        error_set_msg("Unexpected signature 0x%X, expecting 0x%X",
                      (*header_out)->signature, HEADER_SIGNATURE);
        return RETURN_CODE(WPDP_ERROR_FILE_BROKEN);
    }

    return RETURN_CODE(WPDP_OK);
}

int struct_read_section(WPIO_Stream *stream, StructSection **section_out) {
    STRUCT_READ_FIXED(stream, section_out, StructSection);

    if ((*section_out)->signature != SECTION_SIGNATURE) {
        error_set_msg("Unexpected signature 0x%X, expecting 0x%X",
                      (*section_out)->signature, SECTION_SIGNATURE);
        return RETURN_CODE(WPDP_ERROR_FILE_BROKEN);
    }

    return RETURN_CODE(WPDP_OK);
}

int struct_read_node(WPIO_Stream *stream, StructNode **node_out) {
    STRUCT_READ_FIXED(stream, node_out, StructNode);

    if ((*node_out)->signature != NODE_SIGNATURE) {
        error_set_msg("Unexpected signature 0x%X, expecting 0x%X",
                      (*node_out)->signature, NODE_SIGNATURE);
        return RETURN_CODE(WPDP_ERROR);
    }

    return RETURN_CODE(WPDP_OK);
}

int struct_read_metadata(WPIO_Stream *stream, StructMetadata **ptr_out, bool noblob) {
    STRUCT_READ_VARIANT(stream, ptr_out, noblob, StructMetadata,
                        METADATA_SIGNATURE, METADATA_BLOCK_SIZE);

    return RETURN_CODE(WPDP_OK);
}

int struct_read_index_table(WPIO_Stream *stream, StructIndexTable **ptr_out, bool noblob) {
    do {
        StructIndexTable *ptr = wpdp_malloc_zero(INDEX_TABLE_BLOCK_SIZE);
        size_t len = wpio_read(stream, ptr, (size_t)INDEX_TABLE_BLOCK_SIZE);
        if (ptr->signature != INDEX_TABLE_SIGNATURE) {
            error_set_msg("Unexpected signature 0x%X, expecting 0x%X",
                          ptr->signature, INDEX_TABLE_SIGNATURE);
            return RETURN_CODE(WPDP_ERROR_FILE_BROKEN);
        }
        if (noblob) {
            *ptr_out = ptr;
            return RETURN_CODE(WPDP_OK);
        }
        if (ptr->lenBlock > INDEX_TABLE_BLOCK_SIZE) {
            ptr = wpdp_realloc(ptr, ptr->lenBlock);
            len = wpio_read(stream, (ptr + INDEX_TABLE_BLOCK_SIZE), ((size_t)ptr->lenBlock - (size_t)INDEX_TABLE_BLOCK_SIZE));
        }
        *ptr_out = ptr;
    } while (0);

    /*
    STRUCT_READ_VARIANT(stream, ptr_out, noblob, StructIndexTable,
                        INDEX_TABLE_SIGNATURE, INDEX_TABLE_BLOCK_SIZE);
    */

    return RETURN_CODE(WPDP_OK);
}

int struct_write_header(WPIO_Stream *stream, StructHeader *header) {
    STRUCT_WRITE_FIXED(stream, header, StructHeader);

//    WPDP_StreamOperationException::checkIsWriteExactly($len_written, strlen($data_header));

    return RETURN_CODE(WPDP_OK);
}

int struct_write_section(WPIO_Stream *stream, StructSection *section) {
    STRUCT_WRITE_FIXED(stream, section, StructSection);

    return RETURN_CODE(WPDP_OK);
}

int struct_write_node(WPIO_Stream *stream, StructNode *node) {
    STRUCT_WRITE_FIXED(stream, node, StructNode);

    return RETURN_CODE(WPDP_OK);
}

int struct_write_metadata(WPIO_Stream *stream, StructMetadata *metadata) {
    memset((void *)metadata + metadata->lenActual, 0, (size_t)(metadata->lenBlock - metadata->lenActual));

    wpio_write(stream, metadata, (size_t)metadata->lenBlock);

    return RETURN_CODE(WPDP_OK);
}

int struct_write_index_table(WPIO_Stream *stream, StructIndexTable *index_table) {
    memset((void *)index_table + index_table->lenActual, 0, (size_t)(index_table->lenBlock - index_table->lenActual));

    wpio_write(stream, index_table, (size_t)index_table->lenBlock);

    return RETURN_CODE(WPDP_OK);
}

int struct_get_block_length(int block_size, int actual_length) {
    int block_number = (int)ceil((double)actual_length / (double)block_size);
    int block_length = block_size * block_number;

    return block_length;
}
