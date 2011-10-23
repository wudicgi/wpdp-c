#ifndef _STRUCTS_H_
#define _STRUCTS_H_

typedef struct _StructHeader StructHeader;
typedef struct _StructSection StructSection;
typedef struct _StructMetadata StructMetadata;
typedef struct _StructIndexTable StructIndexTable;
typedef struct _StructNode StructNode;

/**
 * 各类型结构的标识常量 (uint32_t)
 */
#define HEADER_SIGNATURE         0x50445057u  // 头信息的标识
#define SECTION_SIGNATURE        0x54434553u  // 区域信息的标识
#define METADATA_SIGNATURE       0x4154454Du  // 元数据的标识
#define INDEX_TABLE_SIGNATURE    0x54584449u  // 索引表的标识
#define NODE_SIGNATURE           0x45444F4Eu  // 结点的标识

/**
 * 属性信息的标识常量 (uint8_t)
 */
#define ATTRIBUTE_SIGNATURE  0xD5u    // 属性信息的标识

/**
 * 索引信息的标识常量 (uint8_t)
 */
#define INDEX_SIGNATURE      0xE1u    // 索引信息的标识

/**
 * 基本块大小常量
 */
#define BASE_BLOCK_SIZE      512 // 基本块大小

/**
 * 各类型结构的块大小常量
 *
 * max_element_size = 2 + 8 + 1 + 255 = 266 (for DATATYPE_STRING)
 * => node_data_size_half >= 266
 * => node_data_size >= 266 * 2 = 532
 * => node_block_size >= 532 + 32 = 564
 * => node_block_size >= 1024 (final min value)
 */
#define HEADER_BLOCK_SIZE        (BASE_BLOCK_SIZE * 1)   // 头信息的块大小
#define SECTION_BLOCK_SIZE       (BASE_BLOCK_SIZE * 1)   // 区域信息的块大小
#define METADATA_BLOCK_SIZE      (BASE_BLOCK_SIZE * 1)   // 元数据的块大小
#define INDEX_TABLE_BLOCK_SIZE   (BASE_BLOCK_SIZE * 1)   // 索引表的块大小
#define NODE_BLOCK_SIZE          (BASE_BLOCK_SIZE * 8)   // 索引结点的块大小

/**
 * 各类型结构的其他大小常量
 */
#define NODE_DATA_SIZE          (NODE_BLOCK_SIZE - 32)          // 索引结点的数据区域大小
#define NODE_DATA_SIZE_EXPANDED ((NODE_BLOCK_SIZE * 2) - 32)    // 扩展的块大小

/**
 * 头信息数据堆版本常量 (uint16_t)
 */
#define HEADER_THIS_VERSION      0x0100u  // 当前数据堆版本

/**
 * 头信息标记常量 (uint16_t)
 */
#define HEADER_FLAG_NONE         0x0000u  // 无任何标记
#define HEADER_FLAG_RESERVED     0x0001u  // 保留标记
#define HEADER_FLAG_READONLY     0x0002u  // 是只读文件

/**
 * 头信息文件类型常量 (uint8_t)
 */
#define HEADER_TYPE_UNDEFINED    0x00u    // 未定义
#define HEADER_TYPE_CONTENTS     0x01u    // 内容文件
#define HEADER_TYPE_METADATA     0x02u    // 元数据文件
#define HEADER_TYPE_INDEXES      0x03u    // 索引文件
#define HEADER_TYPE_COMPOUND     0x10u    // 复合文件 (含内容、元数据与索引)
#define HEADER_TYPE_LOOKUP       0x20u    // 用于查找条目的文件 (含元数据与索引)

/**
 * 头信息文件限制常量 (uint8_t)
 *
 * limits: INT32, UINT32, INT64, UINT64
 *           2GB,    4GB,   8EB,   16EB
 *    PHP:   YES,     NO,    NO,     NO
 *     C#:   YES,    YES,   YES,     NO
 *    C++:   YES,    YES,   YES,     NO
 */
#define HEADER_LIMIT_UNDEFINED   0x00u    // 未定义
#define HEADER_LIMIT_INT32       0x01u    // 文件最大 2GB
#define HEADER_LIMIT_UINT32      0x02u    // 文件最大 4GB (不使用)
#define HEADER_LIMIT_INT64       0x03u    // 文件最大 8EB
#define HEADER_LIMIT_UINT64      0x04u    // 文件最大 16EB (不使用)

/**
 * 区域类型常量 (uint8_t)
 *
 * 为了可以按位组合，方便表示含有哪些区域，采用 2 的整次幂
 */
#define SECTION_TYPE_UNDEFINED   0x00u    // 未定义
#define SECTION_TYPE_CONTENTS    0x01u    // 内容
#define SECTION_TYPE_METADATA    0x02u    // 元数据
#define SECTION_TYPE_INDEXES     0x04u    // 索引

/**
 * 内容压缩类型常量 (uint8_t)
 */
#define CONTENTS_COMPRESSION_NONE    0x00u    // 不压缩
#define CONTENTS_COMPRESSION_GZIP    0x01u    // Gzip
#define CONTENTS_COMPRESSION_BZIP2   0x02u    // Bzip2

/**
 * 内容校验类型常量 (uint8_t)
 */
#define CONTENTS_CHECKSUM_NONE       0x00u    // 不校验
#define CONTENTS_CHECKSUM_CRC32      0x01u    // CRC32
#define CONTENTS_CHECKSUM_MD5        0x02u    // MD5
#define CONTENTS_CHECKSUM_SHA1       0x03u    // SHA1

/**
 * 元数据标记常量 (uint16_t)
 */
#define METADATA_FLAG_NONE           0x0000u  // 无任何标记
#define METADATA_FLAG_RESERVED       0x0001u  // 保留标记
#define METADATA_FLAG_COMPRESSED     0x0010u  // 条目内容已压缩, to be noticed

/**
 * 属性标记常量 (uint8_t)
 */
#define ATTRIBUTE_FLAG_NONE      0x00u    // 无任何标记
#define ATTRIBUTE_FLAG_INDEXED   0x01u    // 索引标记

/**
 * 索引类型常量 (uint8_t)
 */
#define INDEX_TYPE_UNDEFINED     0x00u    // 未定义类型
#define INDEX_TYPE_BTREE         0x01u    // B+ 树类型

/*
in stdint.h:
typedef signed char int8_t
typedef unsigned char uint8_t
typedef signed int int16_t
typedef unsigned int uint16_t
typedef signed long int int32_t
typedef unsigned long int uint32_t
typedef signed long long int int64_t
typedef unsigned long long int uint64_t
*/

/*
typedef struct _wpdp_struct_field {
    uint8_t     number;
    uint8_t     type;
    uint8_t     index;
    uint32_t    ofsRoot;
    uint8_t     lenName;
    char        *name; // 指向 field name 字符串的指针
} WPDP_STRUCT_FIELD;
*/

/*
    ofsNew=CreateNewSpace(offsetof(struct DBEvent,blob)+dbei->cbBlob);

	DBWrite(ofsContact,&dbc,sizeof(struct DBContact));
	DBWrite(ofsNew,&dbe,offsetof(struct DBEvent,blob));
	DBWrite(ofsNew+offsetof(struct DBEvent,blob),dbei->pBlob,dbei->cbBlob);

    static int GetEvent(WPARAM wParam,LPARAM lParam)
*/

#include <pshpack1.h> // for align, important

// fixed
struct _StructHeader {
    uint32_t    signature;      // 块标识
    uint16_t    version;        // 数据堆版本
    uint16_t    flags;          // 数据堆标志
    uint8_t     type;           // 文件类型
    uint8_t     limit;          // 文件限制
    uint8_t     __r_char_1;     // 保留
    uint8_t     __r_char_2;     // 保留
    int64_t     ofsContents;    // 内容区域的偏移量
    int64_t     ofsMetadata;    // 元数据区域的偏移量
    int64_t     ofsIndexes;     // 索引区域的偏移量
    uint8_t     __padding[476]; // 填充块到 512 bytes
};

// fixed
struct _StructSection {
    uint32_t    signature;      // 块标识
    uint8_t     type;           // 区域类型
    uint8_t     __r_char;       // 保留
    int64_t     length;
    int64_t     ofsTable;
    int64_t     ofsFirst;
    uint8_t     __padding[482]; // 填充块到 512 bytes
};

// variant
struct _StructMetadata {
    uint32_t    signature;          // 块标识
    int32_t     lenBlock;           // 块长度
    int32_t     lenActual;          // 实际内容长度
    uint16_t    flags;              // 元数据标记
    uint8_t     compression;        // 压缩算法
    uint8_t     checksum;           // 校验算法
    int64_t     lenOriginal;        // 内容原始长度
    int64_t     lenCompressed;      // 内容压缩后长度
    int32_t     sizeChunk;          // 数据块大小
    int32_t     numChunk;           // 数据块数量
    int64_t     ofsContents;        // 第一个分块的偏移量
    int64_t     ofsOffsetTable;     // 分块偏移量表的偏移量
    int64_t     ofsChecksumTable;   // 分块校验值表的偏移量
    uint8_t     __padding[32];      // 填充块头部到 96 bytes
    uint8_t     blob[];
};

// variant
struct _StructIndexTable {
    uint32_t    signature;      // 块标识
    int32_t     lenBlock;       // 块长度
    int32_t     lenActual;      // 实际内容长度
    uint8_t     __padding[20];  // 填充块头部到 32 bytes
    uint8_t     blob[];
};

// fixed
struct _StructNode {
    uint32_t    signature;      // 块标识
    uint8_t     isLeaf;         // 是否为叶子结点
    uint8_t     __r_char;       // 保留
    uint16_t    numElement;     // 元素数量
    // 对于叶子结点，ofsExtra 为下一个相邻叶子结点的偏移量
    // 对于普通结点，ofsExtra 为比第一个键还要小的键所在结点的偏移量
    int64_t     ofsExtra;       // 补充偏移量 (局部)
    uint8_t     __padding[16];  // 填充块头部到 32 bytes
    uint8_t     blob[NODE_DATA_SIZE];
};

#include <poppack.h>

typedef struct _PacketMetadata  PacketMetadata;
typedef struct _PacketNode      PacketNode;

struct _PacketMetadata {
    StructMetadata  *metadata;
    int64_t         offset;
};

struct _PacketNode {
    StructNode  *node;
    uint8_t     blob_ex[NODE_DATA_SIZE_EXPANDED];
    int         distance_furthest_key;
    int64_t     offset_self;
    int64_t     offset_parent;
};

#endif // _STRUCTS_H_
