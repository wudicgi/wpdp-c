#include "internal.h"

int section_metadata_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out) {
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    int rc = section_init(SECTION_TYPE_METADATA, stream, mode, sect_out);
    RETURN_VAL_IF_NON_ZERO(rc);

    (*sect_out)->custom = NULL;

    return WPDP_OK;
}

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
