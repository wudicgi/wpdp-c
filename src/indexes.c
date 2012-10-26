#include "internal.h"
#include <math.h>

/**
 * 结点缓存参数
 */
#define _NODE_MAX_CACHE     1024    // 最大缓存数量
#define _NODE_AVG_CACHE     768     // 平均缓存数量

#define _BINARY_SEARCH_NOT_FOUND        -127
#define _BINARY_SEARCH_BEYOND_LEFT      -126
#define _BINARY_SEARCH_BEYOND_RIGHT     -125

#define OFFSET_PARENT_NO_NEED   -1  // 不需要父结点的信息
#define OFFSET_PARENT_NULL      -2  // 当前结点已是根结点

#define ELEMENT_KEY_SIZE        ((int)sizeof(uint16_t))
#define ELEMENT_VALUE_SIZE      ((int)sizeof(int64_t))
#define ELEMENT_SIZE            (ELEMENT_KEY_SIZE + ELEMENT_VALUE_SIZE)

#define ELEMENT_KEY_OFFSET      0
#define ELEMENT_VALUE_OFFSET    ELEMENT_KEY_SIZE

/**
 * 获取 p_node 结点中下标为 index 的元素指针 (blob 中)
 */
static void *_std_elem_ptr(PacketNode *p_node, int index) {
    return ((void *)(p_node->node->blob) + (ELEMENT_SIZE * index));
}

/**
 * 获取 p_node 结点中下标为 index 的元素指针 (blob_ex 中)
 */
static void *_ext_elem_ptr(PacketNode *p_node, int index) {
    return ((void *)(p_node->blob_ex) + (ELEMENT_SIZE * index));
}

/*
#define BLOB_EX_KEY_STR_DISTANCE(p_node, i) \
    _com_elem_key_str_distance(_ext_elem_ptr(p_node, i))
*/

/**
 * 获取 ptr_elem 处元素的 key_str 距结尾距离的字节数 (blob 或 blob_ex 中)
 */
static int _com_elem_key_str_distance(void *ptr_elem) {
    return (int)(*((uint16_t *)(ptr_elem + ELEMENT_KEY_OFFSET)));
}

/**
 * 获取 p_node 结点中距结尾 distance 字节处的 key_str 的指针 (blob 中)
 */
static void *_std_elem_key_str_ptr(PacketNode *p_node, int distance) {
    return ((void *)p_node->node->blob + NODE_DATA_SIZE - distance);
}

/**
 * 获取 p_node 结点中距结尾 distance 字节处的 key_str 的指针 (blob_ex 中)
 */
static void *_ext_elem_key_str_ptr(PacketNode *p_node, int distance) {
    return ((void *)p_node->blob_ex + NODE_DATA_SIZE_EXPANDED - distance);
}

/**
 * 获取 p_node 结点中距结尾 distance 字节处的 key_str 的长度 (blob 或 blob_ex 中)
 */
static int _com_elem_key_str_len(void *ptr_key_str) {
    return ((int)(*((int8_t *)ptr_key_str)));
}

/**
 * 获取 p_node 结点中下标为 index 的元素的 key_str (blob_ex 中)
 */
static WPDP_String *_get_element_key(PacketNode *p_node, int index) {
    void *ptr_elem = _ext_elem_ptr(p_node, index);
    void *ptr_key = _ext_elem_key_str_ptr(p_node, _com_elem_key_str_distance(ptr_elem));
    int len_key = _com_elem_key_str_len(ptr_key);

    return wpdp_string_direct(ptr_key + 1, len_key);
}

/**
 * 获取 p_node 结点中下标为 index 的元素的 value (blob_ex 中)
 */
static int64_t _get_element_value(PacketNode *p_node, int index) {
    void *ptr_elem = _ext_elem_ptr(p_node, index);

    return *((int64_t *)(ptr_elem + ELEMENT_VALUE_OFFSET));
}

typedef struct _SectionIndexesCustom Custom;

// Indexes.php: class WPDP_Indexes extends WPDP_Common
struct _SectionIndexesCustom {
    StructIndexTable    *_table;                    // 索引表
    PacketNode  *_p_node_caches[_NODE_MAX_CACHE];   // 结点缓存
    int         _p_node_count;
    int64_t     _offset_end;                        // 当前文件结尾处的偏移量
};

static int64_t _get_offset_root_from_table(Section *sect, WPDP_String *attr_name);

static int _read_table(Section *sect);
static int _write_table(Section *sect);

static int _tree_insert(Section *sect, int64_t root_offset, WPDP_String *key, int64_t value);
static int _split_node(Section *sect, PacketNode *p_node);
static int _split_node_get_parent_node(Section *sect, PacketNode *p_node);
static int _split_node_divide(Section *sect, PacketNode *p_node, PacketNode *node_2, PacketNode *node_parent);
static int _split_node_get_middle(Section *sect, PacketNode *p_node, PacketNode *node_2);
static int _split_node_get_position_in_parent(Section *sect, PacketNode *p_node, PacketNode *node_parent);

static PacketNode *_get_node(Section *sect, int64_t offset, int64_t offset_parent);
static int _create_node(Section *sect, int is_leaf, int64_t offset_parent, PacketNode **p_node_out);

static int _optimize_cache(Section *sect);
static int _flush_nodes(Section *sect);
static int _write_node(Section *sect, PacketNode *p_node);

static int _append_element(PacketNode *p_node, WPDP_String *key, int64_t value);
static int _insert_element_after(PacketNode *p_node, WPDP_String *key, int64_t value, int pos);
static int _is_overflowed(PacketNode *p_node);
static int _compute_node_size(PacketNode *p_node);
static int _compute_element_size(WPDP_String *key);

static int _binary_search_leftmost(PacketNode *p_node, WPDP_String *desired, bool for_lookup);
static int _binary_search_rightmost(PacketNode *p_node, WPDP_String *desired, bool for_insert);

static int _key_compare(PacketNode *p_node, int index, WPDP_String *key);
static WPDP_String *_get_element_key(PacketNode *p_node, int index);
static int64_t _get_element_value(PacketNode *p_node, int index);

/**
 * 构造函数
 *
 * @param object  $stream   文件操作对象
 * @param integer $mode     打开模式
 */
int section_indexes_open(WPIO_Stream *stream, WPDP_OpenMode mode, Section **sect_out) {
    int rc;
    Section *sect;

    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    rc = section_init(SECTION_TYPE_INDEXES, stream, mode, &sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    sect->custom = wpdp_new_zero(Custom, 1);
    if (sect->custom == NULL) {
        return WPDP_ERROR_INTERNAL;
    }
    ((Custom *)sect->custom)->_p_node_count = 0;

    rc = _read_table(sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = section_seek(sect, 0, SEEK_END, _ABSOLUTE); // to be noticed
    RETURN_VAL_IF_NON_ZERO(rc);

    ((Custom *)sect->custom)->_offset_end = section_tell(sect, _RELATIVE);

    *sect_out = sect;

    return RETURN_CODE(WPDP_OK);
}

/**
 * 创建索引文件
 *
 * @param stream    文件操作对象
 */
int section_indexes_create(WPIO_Stream *stream) {
    int rc;
    StructIndexTable *table;
    StructSection *sect;

    rc = section_create(HEADER_TYPE_INDEXES, SECTION_TYPE_INDEXES, stream);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = struct_create_index_table(&table);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = struct_write_index_table(stream, table);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = wpio_seek(stream, HEADER_BLOCK_SIZE, SEEK_SET);
    if (rc == EOF) {
        return WPDP_ERROR;
    }
    rc = struct_read_section(stream, &sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    sect->ofsTable = SECTION_BLOCK_SIZE;

    rc = wpio_seek(stream, HEADER_BLOCK_SIZE, SEEK_SET);
    if (rc == EOF) {
        return WPDP_ERROR;
    }
    rc = struct_write_section(stream, sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    // 写入了重要的结构和信息，将流的缓冲区写入
    rc = wpio_flush(stream);
    if (rc == EOF) {
        return WPDP_ERROR;
    }

    return WPDP_OK;
}

/**
 * 将缓冲内容写入文件
 *
 * 该方法会从缓存中去除某些结点
 */
int section_indexes_flush(Section *sect) {
    /*
    _flush_nodes();
    */

    // to be noticed
    section_seek(sect, 0, SEEK_END, _ABSOLUTE);
    int64_t length = section_tell(sect, _RELATIVE);
    sect->_section->length = length;
    section_write_section(sect, SECTION_TYPE_INDEXES);

    wpio_flush(sect->_stream);

    return WPDP_OK;
}

int64_t section_indexes_get_section_length(Section *sect) {
    Custom *custom = (Custom*)sect->custom;

    return custom->_offset_end;
}

/**
 * 查找符合指定属性值的所有条目元数据的偏移量
 *
 * @param string $attr_name     属性名
 * @param string $attr_value    属性值
 *
 * @return array 返回所有找到的条目元数据的偏移量，未找到任何条目时返回空数组，
 *               指定属性名不存在索引时返回 false
 *
 * @throws WPDP_InternalException
 */
int section_indexes_find(Section *sect, WPDP_String *attr_name, WPDP_String *attr_value) {
    int pos;

    int64_t index_root_offset = _get_offset_root_from_table(sect, attr_name);
    if (index_root_offset == -1) {
        return WPDP_ERROR;
    }

    WPDP_String *key = attr_value;

    int64_t offset = index_root_offset;

    PacketNode *p_node = _get_node(sect, offset, OFFSET_PARENT_NULL);

    while (!p_node->node->isLeaf) {
        pos = _binary_search_leftmost(p_node, key, true);
        if (pos == -1) {
            offset = p_node->node->ofsExtra;
        } else {
            offset = _get_element_value(p_node, pos);
        }

        p_node = _get_node(offset, p_node->offset_self, OFFSET_PARENT_NO_NEED);
    }

    assert(p_node->node->isLeaf);

    pos = _binary_search_leftmost(p_node, key, false);

    if (pos == _BINARY_SEARCH_NOT_FOUND) {
//        return array();
    }

    SectionIndexesOffsets *offsets = NULL;

    while (_key_compare(p_node, pos, key) == 0) {
        SectionIndexesOffsets *offsets_item = wpdp_new_zero(SectionIndexesOffsets, 1);
        offsets_item->offset = _get_element_value(p_node, pos);

        SGLIB_LIST_ADD(SectionIndexesOffsets, offsets, offsets_item, next);

        if (pos < (p_node->node->numElement - 1)) {
            pos++;
        } else if (p_node->node->ofsExtra != 0) {
            p_node = _get_node(sect, p_node->node->ofsExtra, 0/*$this->_node_parents[$node['_ofsSelf']]*/);
            pos = 0;
        } else {
            break;
        }
    }

//    return $offsets;

    return WPDP_OK;
}

/**
 * 从索引表中获取指定索引的偏移量
 */
static int64_t _get_offset_root_from_table(Section *sect, WPDP_String *attr_name) {
    Custom *custom = (Custom*)sect->custom;
    StructIndexTable *table = custom->_table;

    int length = table->lenActual - (int32_t)sizeof(StructIndexTable);

    int pos = 0;
    while (pos < length) {
        // signature
        if (*((uint8_t *)(table->blob + pos)) != INDEX_SIGNATURE) {
            return -1;
        }
        pos += sizeof(uint8_t);

        // type
        if (*((uint8_t *)(table->blob + pos)) != INDEX_TYPE_BTREE) {
            return -1;
        }
        pos += sizeof(uint8_t);

        // name length
        int len = *((uint8_t *)(table->blob + pos));
        pos += sizeof(uint8_t);

        // name string
        WPDP_String *name = wpdp_string_direct(table->blob + pos, len); // to be noticed, possible memory leak
        pos += len;

        // root offset
        if (wpdp_string_compare(name, attr_name) == 0) {
            int64_t offset = *((int64_t *)(table->blob + pos));
            return offset;
        }
        pos += sizeof(int64_t);
    }

    return -1;
}

/**
 * 读取索引表
 */
static int _read_table(Section *sect) {
    Custom *custom = (Custom*)sect->custom;

    section_seek(sect, sect->_section->ofsTable, SEEK_SET, _RELATIVE);
    struct_read_index_table(sect->_stream, &custom->_table, false);

    return RETURN_CODE(WPDP_OK);
}

/**
 * 获取一个结点
 *
 * $offset_parent 参数为 null 时表示要获取的结点是根结点。
 * $offset_parent 参数为缺省值 -1 时表示不需要设置父结点的偏移量信息。
 *
 * 该方法可能会从缓存中去除某些结点
 *
 * @param integer $offset         要获取结点的偏移量 (相对)
 * @param integer $offset_parent  要获取结点父结点的偏移量 (可选)
 *
 * @return array 结点
 */
static PacketNode *_get_node(Section *sect, int64_t offset, int64_t offset_parent) {
    Custom *custom = (Custom*)sect->custom;

    // 只有在 _splitNode_GetParentNode() 方法中，当一个结点不是根结点时，获取其父结点才会使用
    // $offset_parent = OFFSET_PARENT_NO_NEED 的缺省参数，不设置该结点父结点的偏移量信息。
    //
    // index() -- 已对结点进行保护
    // -> _treeInsert() -- 只有在插入元素后结点溢出时才调用分裂结点的方法
    //   -> _splitNode() -- 需要调用获取父结点的方法
    //     -> _splitNode_GetParentNode() -- 该方法只由 _splitNode() 方法调用
    //
    // _treeInsert() 方法在进行向 B+ 树中插入元素的操作时是从树的根结点依次向下进行的，中间
    // 途径结点的父结点偏移量都会被保存下来。而整个过程中涉及到的结点不会被从缓存中除去，所以
    // 在这种情况下使用该缺省参数是安全的。
//    assert((offset_parent != OFFSET_PARENT_NO_NEED) || array_key_exists($offset, $this->_node_parents));

    trace("offset = 0x%llX, parent = 0x%llX", offset, offset_parent);

    int i;
    for (i = 0; i < custom->_p_node_count; i++) {
        if (custom->_p_node_caches[i]->offset_self == offset) {
            trace("found in cache");
            return custom->_p_node_caches[i];
        }
    }

/*
    if (offset_parent != OFFSET_PARENT_NO_NEED) {
        $this->_node_parents[$offset] = $offset_parent;
    }
*/

    trace("read from file");

    section_seek(sect, offset, SEEK_SET, _RELATIVE);

    PacketNode *p_node = wpdp_new_zero(PacketNode, 1);
    struct_read_node(sect->_stream, &p_node->node);
    p_node->offset_self = offset;
    p_node->offset_parent = offset_parent;

    /* 将 blob 中的数据扩展复制到 blob_ex 中 */
    void *ptr_last_elem = _ext_elem_ptr(p_node, p_node->node->numElement - 1);
    int distance_last_key = _com_elem_key_str_distance(ptr_last_elem);
    // 复制前端的 value (偏移量)
    memcpy(p_node->blob_ex, p_node->node->blob, (size_t)(ELEMENT_SIZE * p_node->node->numElement));
    // 复制后端的 key_str (字符串)
    memcpy(_ext_elem_key_str_ptr(p_node, distance_last_key),
        _std_elem_key_str_ptr(p_node, distance_last_key),
        (size_t)distance_last_key);

    p_node->distance_furthest_key = distance_last_key;

    custom->_p_node_caches[custom->_p_node_count] = p_node;
    custom->_p_node_count++;

    return p_node;
}

/**
 * 查找指定键在结点中的最左元素的位置
 *
 *       [0][1][2][3][4][5][6]
 * keys = 2, 3, 3, 5, 7, 7, 8
 *
 * +---------+----------+------------+
 * | desired | found at | for lookup |
 * +---------+----------+------------+
 * |       1 |        / |         -1 |
 * |       2 |        0 |          0 |
 * |       3 |        1 |          1 |
 * |       4 |        / |          2 |
 * |       5 |        3 |          3 |
 * |       6 |        / |          3 |
 * |       7 |        4 |          4 |
 * |       8 |        6 |          6 |
 * |       9 |        / |          6 |
 * +---------+----------+------------+
 *
 * 算法主要来自 Patrick O'Neil 和 Elizabeth O'Neil 所著的 Database: Principles,
 * Programming, and Performance (Second Edition) 中的 Example 8.3.1
 *
 * @param array  $node        结点
 * @param string $desired     要查找的键
 * @param bool   $for_lookup  是否用于查找元素目的
 *
 * @return integer 位置
 */
static int _binary_search_leftmost(PacketNode *p_node, WPDP_String *desired, bool for_lookup) {
    trace("desired = %s, %s", desired->str, (for_lookup ? ", for lookup" : ""));

    int count = p_node->node->numElement;

    if (count == 0 || _key_compare(p_node, count - 1, desired) < 0) {
        trace("out of right bound");
        return (for_lookup ? (count - 1) : _BINARY_SEARCH_NOT_FOUND);
    } else if (_key_compare(p_node, 0, desired) > 0) {
        trace("out of left bound");
        return (for_lookup ? -1 : _BINARY_SEARCH_NOT_FOUND);
    }

    int m = (int)ceil(log2(count));
    int probe = (int)(pow(2, m - 1) - 1);
    int diff = (int)pow(2, m - 2);

    while (diff > 0) {
        trace("probe = %d (diff = %d)", probe, diff);
        if (probe < count && _key_compare(p_node, probe, desired) < 0) {
            probe += diff;
        } else {
            probe -= diff;
        }
        // diff 为正数，不必再加 floor()
        diff = (int)(diff / 2);
    }

    trace("probe = %d (diff = %d)", probe, diff);

    if (probe < count && _key_compare(p_node, probe, desired) == 0) {
        return probe;
    } else if (probe + 1 < count && _key_compare(p_node, probe + 1, desired) == 0) {
        return probe + 1;
    } else if (for_lookup && probe < count && _key_compare(p_node, probe, desired) > 0) {
        return probe - 1;
    } else if (for_lookup && probe + 1 < count && _key_compare(p_node, probe + 1, desired) > 0) {
        return probe;
    } else {
        return _BINARY_SEARCH_NOT_FOUND;
    }
}

/**
 * 比较结点中指定下标元素的键与另一个给定键的大小
 *
 * @param node      结点
 * @param index     key1 在结点元素数组中的下标
 * @param key       key2
 *
 * @return  如果 key1 小于 key2，返回 < 0 的值，大于则返回 > 0 的值，
 *          等于则返回 0
 */
static int _key_compare(PacketNode *p_node, int index, WPDP_String *key) {
//    assert('array_key_exists($index, $node[\'elements\'])');

    WPDP_String *key_in_elem = _get_element_key(p_node, index);

    return wpdp_string_compare(key_in_elem, key);
}
