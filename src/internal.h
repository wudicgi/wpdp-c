#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
//#include <types.h>
#include <stdint.h>
#include <stdbool.h>

#include "wpio.h"

typedef enum _OffsetType    OffsetType;

typedef struct _Section         Section;

typedef struct _WPDP_Entry_Args     WPDP_Entry_Args;

#include "error.h"
#include "malloc.h"
#include "structs.h"
#include "wpdp.h"

// 定位方式常量
enum _OffsetType {
    _ABSOLUTE,  // 绝对偏移量定位
    _RELATIVE   // 相对偏移量定位
};

// Common.php: abstract class WPDP_Common
struct _Section {
    /* 通用属性 */
    WPDP_OpenMode   _open_mode;     // 文件打开模式
    WPIO_Stream     *_stream;       // 文件操作对象
    off64_t         _offset_base;   // 偏移量基数
    StructHeader    *_header;       // 头信息
    StructSection   *_section;      // 区域信息
    /* 附加信息 */
    uint8_t         type;
    void            *custom;
};

// Entry.php: class WPDP_Entry_Args extends WPDP_Entry_Information
struct _WPDP_Entry_Args {
    // extends entry info
    WPDP_Entry_Info         entry_info;
    // contents
    int64_t                 contents_offset;        // 第一个分块的偏移量
    int64_t                 offset_table_offset;    // 分块偏移量表的偏移量
    int64_t                 checksum_table_offset;  // 分块校验值表的偏移量
    // metadata
    int64_t                 metadata_offset;        // 元数据的偏移量
    WPDP_Entry_Attributes   *attributes;            // 条目属性
};

/* entry */

WPDP_Entry_Attributes *wpdp_entry_attributes_create(void);
int wpdp_entry_attributes_add(WPDP_Entry_Attributes *attrs, WPDP_Entry_Attribute *attr);
int wpdp_entry_attributes_free(WPDP_Entry_Attributes *attrs);

WPDP_Entry_Attribute *wpdp_entry_attribute_create(WPDP_String *name, WPDP_String *value, bool is_index);

/* section */

int section_init(uint8_t sect_type, WPIO_Stream *stream,
                 WPDP_OpenMode mode, Section **sect_out);
int section_create(uint8_t file_type, uint8_t sect_type,
                   WPIO_Stream *stream);
WPIO_Stream *section_get_stream(Section *sect);
int section_read_header(Section *sect);
int section_write_header(Section *sect);
int section_read_section(Section *sect, uint8_t sect_type);
int section_write_section(Section *sect, uint8_t sect_type);
int64_t section_tell(Section *sect, OffsetType offset_type);
int section_seek(Section *sect, int64_t offset, int origin, OffsetType offset_type);
int section_read(Section *sect, void *buffer, int length);
int section_write(Section *sect, void *buffer, int length);

/* contents */

int section_contents_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out);
int section_contents_create(WPIO_Stream *stream);
int section_contents_flush(Section *sect);
int64_t section_contents_get_section_length(Section *sect);
int section_contents_begin(Section *sect, int64_t length, WPDP_Entry_Args *args);
int section_contents_transfer(Section *sect, WPDP_Entry_Args *args, const void *data, int32_t len);
int section_contents_commit(Section *sect, WPDP_Entry_Args *args);

/* metadata */

int section_metadata_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out);
int section_metadata_create(WPIO_Stream *stream);
int section_metadata_flush(Section *sect);
int64_t section_metadata_get_section_length(Section *sect);
int section_metadata_add(Section *sect, WPDP_Entry_Args *args);

/* indexes */

int section_indexes_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out);
int section_indexes_create(WPIO_Stream *stream);
int section_indexes_flush(Section *sect);
int64_t section_indexes_get_section_length(Section *sect);
void *section_indexes_find(Section *sect, WPDP_String *attr_name, WPDP_String *attr_value);
int section_indexes_index(Section *sect, WPDP_Entry_Args *args);

/* struct */

int struct_create_header(StructHeader **header_out);
int struct_create_section(StructSection **section_out);
int struct_create_metadata(StructMetadata **metadata_out);
int struct_create_index_table(StructIndexTable **index_table_out);
int struct_create_node(StructNode **node_out);

int struct_read_header(WPIO_Stream *stream, StructHeader **header_out);
int struct_read_section(WPIO_Stream *stream, StructSection **section_out);
int struct_read_node(WPIO_Stream *stream, StructNode **node_out);
int struct_read_metadata(WPIO_Stream *stream, StructMetadata **ptr_out, bool noblob);
int struct_read_index_table(WPIO_Stream *stream, StructIndexTable **ptr_out, bool noblob);

int struct_write_header(WPIO_Stream *stream, StructHeader *header);
int struct_write_section(WPIO_Stream *stream, StructSection *section);
int struct_write_node(WPIO_Stream *stream, StructNode *node);
int struct_write_metadata(WPIO_Stream *stream, StructMetadata *metadata);
int struct_write_index_table(WPIO_Stream *stream, StructIndexTable *index_table);

int struct_get_block_length(int block_size, int actual_length);

Section     *contents_open(WPIO_Stream *stream);

void indexes_create(WPIO_Stream *stream);

#endif // _INTERNAL_H_
