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

static void *_std_elem_ptr(PacketNode *p_node, int index) {
    return ((void *)(p_node->node->blob) + (ELEMENT_SIZE * index));
}

static void *_ext_elem_ptr(PacketNode *p_node, int index) {
    return ((void *)(p_node->blob_ex) + (ELEMENT_SIZE * index));
}
/*
#define BLOB_EX_KEY_STR_DISTANCE(p_node, i) \
    _com_elem_key_str_distance(_ext_elem_ptr(p_node, i))
*/
static int _com_elem_key_str_distance(void *ptr_elem) {
    return (int)(*((uint16_t *)(ptr_elem + ELEMENT_KEY_OFFSET)));
}

static void *_std_elem_key_str_ptr(PacketNode *p_node, int distance) {
    return ((void *)p_node->node->blob + NODE_DATA_SIZE - distance);
}

static void *_ext_elem_key_str_ptr(PacketNode *p_node, int distance) {
    return ((void *)p_node->blob_ex + NODE_DATA_SIZE_EXPANDED - distance);
}

static int _com_elem_key_str_len(void *ptr_key_str) {
    return ((int)(*((int8_t *)ptr_key_str)));
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

#ifndef BUILD_READONLY
    static int _modify_table_add(Section *sect, WPDP_String *attr_name, int64_t offset_root);
#endif
static int _read_table(Section *sect);
#ifndef BUILD_READONLY
    static int _write_table(Section *sect);
#endif

#ifndef BUILD_READONLY
    static int _tree_insert(Section *sect, int64_t root_offset, WPDP_String *key, int64_t value);
    static bool _split_node(Section *sect, PacketNode *p_node);
    static PacketNode *_split_node_get_parent_node(Section *sect, PacketNode *p_node);
    static void _split_node_divide(Section *sect, PacketNode *p_node,
                                   PacketNode *p_node_2, PacketNode *p_node_parent);
    static int _split_node_get_middle(Section *sect, PacketNode *p_node, PacketNode *p_node_2);
    static int _split_node_get_position_in_parent(Section *sect, PacketNode *p_node, PacketNode *p_node_parent);
#endif

static PacketNode *_get_node(Section *sect, int64_t offset, int64_t offset_parent);
#ifndef BUILD_READONLY
    static PacketNode *_create_node(Section *sect, bool is_leaf, int64_t offset_parent);
    static bool _flush_nodes(Section *sect);
    static int _write_node(Section *sect, PacketNode *p_node);
    static bool _append_element(PacketNode *p_node, WPDP_String *key, int64_t value);
    static bool _insert_element_after(PacketNode *p_node, WPDP_String *key, int64_t value, int pos);
    static bool _is_overflowed(PacketNode *p_node);
    static int _compute_node_size(PacketNode *p_node);
    static int _compute_element_size(WPDP_String *key);
#endif

static int _binary_search_leftmost(PacketNode *p_node, WPDP_String *desired, bool for_lookup);
#ifndef BUILD_READONLY
    static int _binary_search_rightmost(PacketNode *p_node, WPDP_String *desired, bool for_insert);
#endif

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
    assert(IN_ARRAY_2(mode, WPDP_MODE_READONLY, WPDP_MODE_READWRITE));

    Section *sect;
    int rc = section_init(SECTION_TYPE_INDEXES, stream, mode, &sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    sect->custom = wpdp_new_zero(Custom, 1);
    ((Custom *)sect->custom)->_p_node_count = 0;

    rc = _read_table(sect);
    RETURN_VAL_IF_NON_ZERO(rc);

    rc = section_seek(sect, 0, SEEK_END, _ABSOLUTE); // to be noticed
    RETURN_VAL_IF_NON_ZERO(rc);

    ((Custom *)sect->custom)->_offset_end = section_tell(sect, _RELATIVE);

    *sect_out = sect;

    return RETURN_CODE(WPDP_OK);
}

#ifndef BUILD_READONLY

/**
 * 创建索引文件
 *
 * @param object $stream    文件操作对象
 */
int section_indexes_create(WPIO_Stream *stream) {
    section_create(HEADER_TYPE_INDEXES, SECTION_TYPE_INDEXES, stream);

    StructIndexTable *index_table;
    struct_create_index_table(&index_table);
    index_table->lenActual = sizeof(StructIndexTable);
    index_table->lenBlock = INDEX_TABLE_BLOCK_SIZE;
    index_table = wpdp_realloc(index_table, index_table->lenBlock);
    struct_write_index_table(stream, index_table);

    wpio_seek(stream, HEADER_BLOCK_SIZE, SEEK_SET);
    StructSection *section;
    struct_read_section(stream, &section);
    section->ofsTable = SECTION_BLOCK_SIZE;

    wpio_seek(stream, HEADER_BLOCK_SIZE, SEEK_SET);
    struct_write_section(stream, section);

    // 写入了重要的结构和信息，将流的缓冲区写入
    wpio_flush(stream);

    wpdp_free(index_table);
    wpdp_free(section);

    return RETURN_CODE(WPDP_OK);
}

#endif

#ifndef BUILD_READONLY

/**
 * 将缓冲内容写入文件
 *
 * 该方法会从缓存中去除某些结点
 */
int section_indexes_flush(Section *sect) {
//    $this->_flushNodes();

    // to be noticed
    section_seek(sect, 0, SEEK_END, _ABSOLUTE);
    int64_t length = section_tell(sect, _RELATIVE);
    sect->_section->length = length;
    section_write_section(sect);

    wpio_flush(sect->_stream);

    return RETURN_CODE(WPDP_OK);
}

#endif

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
void *section_indexes_find(Section *sect, WPDP_String *attr_name, WPDP_String *attr_value) {
    // Possible traces:
    // EXTERNAL -> find()
    //
    // So this method NEED to protect the nodes in cache
/*
    if (!array_key_exists($attr_name, $this->_table['indexes'])) {
        return false;
    }
*/
#ifndef BUILD_READONLY
//    $this->_beginNodeProtection();
#endif

    WPDP_String *key = attr_value;

    int64_t offset = 0; // $this->_table['indexes'][$attr_name]['ofsRoot'];

    PacketNode *p_node = _get_node(sect, offset, OFFSET_PARENT_NULL);

    while (!p_node->node->isLeaf) {
        int pos = _binary_search_leftmost(p_node, key, true);
        if (pos == -1) {
            offset = p_node->node->ofsExtra;
        } else {
//            offset = node['elements'][$pos]['value'];
        }

//        p_node = _get_node(offset, $node['_ofsSelf']);
    }

    int pos = _binary_search_leftmost(p_node, key, false);

    if (pos == _BINARY_SEARCH_NOT_FOUND) {
//        return array();
    }

//    offsets = array();
/*
    while ($node['elements'][$pos]['key'] == $key) {
        $offsets[] = $node['elements'][$pos]['value'];

        if ($pos < count($node['elements']) - 1) {
            $pos++;
        } elseif ($node['ofsExtra'] != 0) {
            $node =& $this->_getNode($node['ofsExtra'], $this->_node_parents[$node['_ofsSelf']]);
            $pos = 0;
        } else {
            break;
        }
    }

#ifndef BUILD_READONLY
    $this->_endNodeProtection();
#endif

    return $offsets;
*/

    return NULL;
}

#ifndef BUILD_READONLY

/**
 * 对指定条目做索引
 *
 * @param object $args  条目参数
 *
 * @return bool 总是 true
 */
void *section_indexes_index(Section *sect, WPDP_Entry_Args *args) {
    // Possible traces:
    // EXTERNAL -> index()
    //
    // So this method NEED to protect the nodes in cache

//    $this->_beginNodeProtection();

    // 处理该条目属性中需索引的项目
    WPDP_Entry_Attributes *attrs = args->attributes;

    int i;
    for (i = 0; i < attrs->size; i++) {
        WPDP_Entry_Attribute *attr = attrs->attrs[i];

        if (!attr->index) {
            continue;
        }

        int64_t offset = _get_offset_root_from_table(sect, attr->name);

        if (offset == -1) {
            PacketNode *p_node_root = _create_node(sect, true, OFFSET_PARENT_NULL);
            _flush_nodes(sect);
            _modify_table_add(sect, attr->name, p_node_root->offset_self);
            // 写入了重要的结构和信息，将流的缓冲区写入
            section_indexes_flush(sect); // to be noticed
            offset = p_node_root->offset_self;
        }

        trace("offset = 0x%llX", offset);

        _tree_insert(sect, offset, attr->value, args->metadata_offset);
    }

    _flush_nodes(sect);

//    $this->_endNodeProtection();

    return NULL;
}

#endif

static int64_t _get_offset_root_from_table(Section *sect, WPDP_String *attr_name) {
    Custom *custom = (Custom*)sect->custom;
    StructIndexTable *table = custom->_table;

    int length = table->lenActual - (int32_t)sizeof(StructIndexTable);

    int pos = 0;
    while (pos < length) {
        if (*((uint8_t *)(table->blob + pos)) != INDEX_SIGNATURE) {
            return -1;
        }
        pos++;

        if (*((uint8_t *)(table->blob + pos)) != INDEX_TYPE_BTREE) {
            return -1;
        }
        pos++;

        int len = *((uint8_t *)(table->blob + pos));
        pos++;

        WPDP_String *name = wpdp_string_direct(table->blob + pos, len);
        pos += len;

        if (wpdp_string_compare(name, attr_name) == 0) {
            int64_t offset = *((int64_t *)(table->blob + pos));
            return offset;
        }
        pos += 8;
    }

    return -1;
}

#ifndef BUILD_READONLY

static int _modify_table_add(Section *sect, WPDP_String *attr_name, int64_t offset_root) {
    Custom *custom = (Custom*)sect->custom;
    StructIndexTable *table = custom->_table;

    uint8_t signature = INDEX_SIGNATURE;
    uint8_t type = INDEX_TYPE_BTREE;
    uint8_t attr_name_len = attr_name->len;

    WPDP_String_Builder *builder = wpdp_string_builder_create(256);
    wpdp_string_builder_append(builder, &signature, sizeof(signature));
    wpdp_string_builder_append(builder, &type, sizeof(type));
    wpdp_string_builder_append(builder, &attr_name_len, sizeof(attr_name_len));
    wpdp_string_builder_append(builder, attr_name->str, attr_name->len);
    wpdp_string_builder_append(builder, &offset_root, sizeof(offset_root));

    int32_t len_actual_orig = table->lenActual;
    int32_t len_block_orig = table->lenBlock;

    table->lenActual += builder->length;
    table->lenBlock = struct_get_block_length(INDEX_TABLE_BLOCK_SIZE, table->lenActual);

    if (table->lenBlock > len_block_orig) {
        table = wpdp_realloc(table, table->lenBlock);
        custom->_table = table;
        memcpy((void *)table + len_actual_orig, builder->str, (size_t)builder->length);

        section_seek(sect, 0, SEEK_END, _ABSOLUTE);
        int64_t offset_new = section_tell(sect, _RELATIVE);
        struct_write_index_table(sect->_stream, table);

        custom->_offset_end += table->lenBlock;

        sect->_section->ofsTable = offset_new;
        section_write_section(sect);
    } else {
        memcpy((void *)table + len_actual_orig, builder->str, (size_t)builder->length);

        section_seek(sect, sect->_section->ofsTable, SEEK_SET, _RELATIVE);
        struct_write_index_table(sect->_stream, table);
    }

    wpdp_string_builder_free(builder);

    return RETURN_CODE(WPDP_OK);
}

#endif

/**
 * 读取索引表
 */
static int _read_table(Section *sect) {
    Custom *custom = (Custom*)sect->custom;

    section_seek(sect, sect->_section->ofsTable, SEEK_SET, _RELATIVE);
    struct_read_index_table(sect->_stream, &custom->_table, false);

    return RETURN_CODE(WPDP_OK);
}

#ifndef BUILD_READONLY

/**
 * 写入索引表
 */
static int _write_table(Section *sect) {
    Custom *custom = (Custom*)sect->custom;

    section_seek(sect, sect->_section->ofsTable, SEEK_SET, _RELATIVE);
    struct_write_index_table(sect->_stream, custom->_table);
}

#endif

#ifndef BUILD_READONLY

/**
 * 插入指定结点到 B+ 树中
 *
 * @param integer $root_offset  B+ 树根结点的偏移量
 * @param string  $key          结点的键 (用于查找的数值或字符串)
 * @param integer $value        结点的值 (条目元数据的相对偏移量)
 *
 * @return bool 总是 true
 */
static int _tree_insert(Section *sect, int64_t root_offset, WPDP_String *key, int64_t value) {
    trace("root_offset = 0x%llX, key = %s, value = 0x%llX", root_offset, key->str, value);

    // Possible traces:
    // ... -> index() [PROTECTED] -> _treeInsert()
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    // 当前结点的偏移量
    int64_t offset = root_offset;
    // 当前结点
    PacketNode *p_node = _get_node(sect, offset, OFFSET_PARENT_NULL);

    while (!p_node->node->isLeaf) {
        trace("go through node 0x%llX", p_node->offset_self);
        int pos = _binary_search_rightmost(p_node, key, true);
        if (pos == -1) {
            offset = p_node->node->ofsExtra;
        } else {
            offset = _get_element_value(p_node, pos);
        }

        p_node = _get_node(sect, offset, p_node->offset_self);
    }

    assert(p_node->node->isLeaf == true);

    trace("now at the leaf node 0x%llX", p_node->offset_self);

    int pos = _binary_search_rightmost(p_node, key, true);

//    assert('array_key_exists($node[\'_ofsSelf\'], $this->_node_caches)');

    _insert_element_after(p_node, key, value, pos);

    if (_is_overflowed(p_node)) {
        _split_node(sect, p_node);
    }

    return RETURN_CODE(WPDP_OK);
}

#endif

#ifndef BUILD_READONLY

/**
 * 分裂结点
 *
 * @param array $node   结点
 *
 * @return bool 总是 true
 */
static bool _split_node(Section *sect, PacketNode *p_node) {
    assert(_is_overflowed(p_node) == true);
    assert(p_node->node->numElement >= 2);

    trace("node_offset = 0x%llX, is_leaf = %s", p_node->offset_self, (p_node->node->isLeaf ? "true" : "false"));

    // Possible traces:
    // ... -> _treeInsert() [PROTECTED] -> _splitNode()
    // ... -> _treeInsert() [PROTECTED] -> _splitNode() -> _splitNode() [-> ...]
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    /*
    $count_elements = count($node['elements']);
    $node_size_orig = $node['_size']; // for test, to be noticed
    */

    PacketNode *p_node_parent = _split_node_get_parent_node(sect, p_node);

    // 创建新的下一个结点, to be noticed
    PacketNode *p_node_2 = _create_node(sect, p_node->node->isLeaf,
        p_node->offset_parent);

    _split_node_divide(sect, p_node, p_node_2, p_node_parent);

    assert(_is_overflowed(p_node) == false);
    assert(_is_overflowed(p_node_2) == false);

    /*
    trace(__METHOD__, "split a node, size: $node_size_orig => " . $node['_size'] . " + " . $node_2['_size'] . ", count: $count_elements => " . count($node['elements']) . " + " . count($node_2['elements']) . "\n");
    */

    if (_is_overflowed(p_node_parent)) {
        _split_node(sect, p_node_parent);
    }
}

#endif

#ifndef BUILD_READONLY

static PacketNode *_split_node_get_parent_node(Section *sect, PacketNode *p_node) {
    // Possible traces:
    // ... -> _splitNode() [PROTECTED] -> _splitNode_GetParentNode()
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    // 若当前结点不是根结点，直接获取其父结点并返回
    if (p_node->offset_parent != OFFSET_PARENT_NULL) {
        trace("the node to split has parent node");
        PacketNode *p_node_parent = _get_node(sect, p_node->offset_parent, OFFSET_PARENT_NO_NEED);
        return p_node_parent;
    }

    // 当前结点为根结点
    trace("the node to split is the root node");
    // 创建新的根结点
    PacketNode *p_node_parent = _create_node(sect, false, OFFSET_PARENT_NULL);
    // 设置当前结点的父结点为新创建的根结点
    p_node->offset_parent = p_node_parent->offset_self;
    // 将当前结点的首个元素的键添加到新建的根结点中
    trace("add offset 0x%llX as the new root's ofsExtra", p_node->offset_self);
//    assert('array_key_exists($node_parent[\'_ofsSelf\'], $this->_node_caches)');
    _append_element(p_node_parent, _get_element_key(p_node, 0),
        p_node->offset_self);

    // to be noticed
    bool flag_changed = false;
    /*
    foreach ($this->_table['indexes'] as &$index) {
        if ($index['ofsRoot'] == $node['_ofsSelf']) {
            $index['ofsRoot'] = $node_parent['_ofsSelf'];
            $flag_changed = true;
            trace(__METHOD__, "change the root of index $index[name] to " . $node_parent['_ofsSelf']);
            break;
        }
    }
    unset($index);
    */
    assert(flag_changed == true);
    _write_table(sect);

    // 写入了重要的结构和信息，将流的缓冲区写入
    section_indexes_flush(sect); // to be noticed

    return p_node_parent;
}

#endif

#ifndef BUILD_READONLY

static void _split_node_divide(Section *sect, PacketNode *p_node,
                               PacketNode *p_node_2, PacketNode *p_node_parent) {
    // Possible traces:
    // ... -> _splitNode() [PROTECTED] -> _splitNode_Divide()
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    int middle = 0;
    int node_size_left = 0;
//    list ($middle, $node_size_left) = _split_node_get_middle(p_node, p_node_2);

    // 获取当前结点在父结点中的位置
    int node_pos_in_parent = _split_node_get_position_in_parent(sect, p_node, p_node_parent);
    trace("position in parent is %d", node_pos_in_parent);

    // 叶子结点和普通结点的分裂方式不同
    if (p_node->node->isLeaf) {
        trace("the node to split is a leaf node");

        // 设置新建的同级结点和当前结点的下一个结点偏移量信息
        p_node_2->node->ofsExtra = p_node->node->ofsExtra;
        p_node->node->ofsExtra = p_node_2->offset_self;
/*
        $node_2['elements'] = array_slice($node['elements'], $middle);
        $node['elements'] = array_slice($node['elements'], 0, $middle);

        $node_2['_size'] = $node['_size'] - $node_size_left;
        $node['_size'] = $node_size_left;
*/
//        assert('array_key_exists($node_parent[\'_ofsSelf\'], $this->_node_caches)');

        _insert_element_after(p_node_parent, _get_element_key(p_node_2, 0),
            p_node_2->offset_self, node_pos_in_parent);
    } else {
        trace("the node to split is an ordinary node");

        WPDP_String *element_mid_key = _get_element_key(p_node, middle);
        int64_t element_mid_value = _get_element_value(p_node, middle);
        p_node_2->node->ofsExtra = element_mid_value;
        /*
        $node_2['elements'] = array_slice($node['elements'], $middle + 1);
        $node['elements'] = array_slice($node['elements'], 0, $middle);
        $node_2['_size'] = $node['_size'] - $node_size_left;
        $node_2['_size'] -= self::_computeElementSize(element_mid_key);
        $node['_size'] = $node_size_left;
        */

        // newly added, fixed the bug
        /*
        $this->_node_parents[$node_2['ofsExtra']] = $node_2['_ofsSelf'];
        foreach ($node_2['elements'] as $elem) {
            $this->_node_parents[$elem['value']] = $node_2['_ofsSelf'];
        }
        */

//        assert('array_key_exists($node_parent[\'_ofsSelf\'], $this->_node_caches)');

        _insert_element_after(p_node_parent, element_mid_key,
            p_node_2->offset_self, node_pos_in_parent);
    }
}

#endif

#ifndef BUILD_READONLY

static int _split_node_get_middle(Section *sect, PacketNode *p_node, PacketNode *p_node_2) {
    // Possible traces:
    // ... -> _splitNode_Divide() [PROTECTED] -> _splitNode_GetMiddle()
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    int count_elements = p_node->node->numElement;

    int node_size_orig = _compute_node_size(p_node);
    int node_size_half = (int)(NODE_DATA_SIZE / 2);
    int node_size_left = 0;

    trace("size_orig = %d, size_half = %d", node_size_orig, node_size_half);
    trace("size_left = %d", node_size_left);

    int middle = -1;
    int pos;
    for (pos = 0; pos < count_elements; pos++) {
        int elem_size = _compute_element_size(_get_element_key(p_node, pos));
        trace("size_elem[%d] = %d", pos, elem_size);
        if (node_size_left + elem_size > node_size_half) {
            trace("size_left + size_elem = %d > size_half", (node_size_left + elem_size));
            middle = pos;
            break;
        }
        trace("size_left = size_left + size_elem = %d", (node_size_left + elem_size));
        node_size_left += elem_size;
    }

    assert(middle != -1); // to be noticed
    assert(middle != count_elements); // to be noticed

    // 情况 1)
    //
    // A A A A B B B B
    //       ^ middle = 3
    //
    // 若中间键和第一个键相同，不用处理
    //
    // 情况 2)
    //
    // A A A B B B B B
    //       ^ middle = 3
    //
    // 若中间键和第一个键不同，但中间键和其前一个键不同，则也不用处理
    // 此时底下代码的 while 循环不会起作用
    //
    // 情况 3)
    //
    // A A B B B B B B
    //       ^ middle = 3
    //
    // 若中间键和第一个键不同，while 循环会尝试找到和中间键相同的最靠左的键
    // 对于上例，最终结果为 middle = 2

    if (wpdp_string_compare(_get_element_key(p_node, middle), _get_element_key(p_node, 0)) != 0) {
        while (wpdp_string_compare(_get_element_key(p_node, middle), _get_element_key(p_node, middle - 1)) == 0) {
            middle--;
            node_size_left -= _compute_element_size(_get_element_key(p_node, middle));
        }
    }

    assert(middle > 0); // to be noticed

//    return array($middle, $node_size_left);
    return 0;
}

#endif

#ifndef BUILD_READONLY

/**
 * 获取指定结点在其父结点中的位置
 *
 * @param array $node           结点
 * @param array $node_parent    父结点
 *
 * @return integer 位置
 */
static int _split_node_get_position_in_parent(Section *sect, PacketNode *p_node, PacketNode *p_node_parent) {
    assert(p_node->node->numElement > 0); // 需要利用结点中的第一个键进行查找
    assert(p_node_parent->node->isLeaf == false);

    trace("node_offset = 0x%llX", p_node->offset_self);

    // Possible traces:
    // ... -> _splitNode_Divide() [PROTECTED] -> _splitNode_GetPositionInParent()
    //
    // So this method needn't and shouldn't to protect the nodes in cache

    int64_t offset = p_node->offset_self;

    if (p_node_parent->node->ofsExtra == offset) {
        trace("found node offset at ofsExtra");
        // 若当前结点为父结点的最左边的子结点
        return -1;
    }

    // 此处需要使用 lookup 方式在父结点中查找结点的第一个键
    // B+ 树的形态参考 Database 一书中的图 8-12
    int pos = _binary_search_leftmost(p_node_parent, _get_element_key(p_node, 0), true);

    assert(pos != -1); // 若位置在左边界外，则应在前面的 if 判断中已检查出

    int count_parent = p_node_parent->node->numElement;

    // 从查找到的位置向右依次判断
    while (pos < count_parent) {
        if (_get_element_value(p_node_parent, pos) == offset) {
            trace("found node offset at pos %d", pos);
            return pos;
        }
        pos++;
    }

    // 在父结点中没有找到当前结点，抛出异常
//    return WPDP_ERROR_FILE_BROKEN;
}

#endif

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

    void *ptr_last_elem = _ext_elem_ptr(p_node, p_node->node->numElement - 1);
    int distance_last_key = _com_elem_key_str_distance(ptr_last_elem);
    memcpy(p_node->blob_ex, p_node->node->blob, (size_t)(ELEMENT_SIZE * p_node->node->numElement));
    memcpy(_ext_elem_key_str_ptr(p_node, distance_last_key),
        _std_elem_key_str_ptr(p_node, distance_last_key),
        (size_t)distance_last_key);

    p_node->distance_furthest_key = distance_last_key;

    custom->_p_node_caches[custom->_p_node_count] = p_node;
    custom->_p_node_count++;

    return p_node;
}

#ifndef BUILD_READONLY

/**
 * 创建一个结点
 *
 * 该方法可能会从缓存中去除某些结点
 *
 * @param bool $is_leaf             是否为叶子结点
 * @param integer $offset_parent    父结点的偏移量
 *
 * @return array 结点
 */
static PacketNode *_create_node(Section *sect, bool is_leaf, int64_t offset_parent) {
    Custom *custom = (Custom*)sect->custom;

    trace("is_leaf = %d, parent = 0x%llX", is_leaf, offset_parent);

    StructNode *node = NULL;
    struct_create_node(&node);

    node->isLeaf = (uint8_t)is_leaf;
    node->numElement = 0;

    trace("offset_end = 0x%llX", custom->_offset_end);

    int64_t offset = custom->_offset_end;
    custom->_offset_end += NODE_BLOCK_SIZE;

    trace("node created at 0x%llX", offset);

    PacketNode *p_node = wpdp_new_zero(PacketNode, 1);
    p_node->node = node;
    p_node->offset_self = offset;
    p_node->offset_parent = offset_parent;

    custom->_p_node_caches[custom->_p_node_count] = p_node;
    custom->_p_node_count++;

    return p_node;
}

#endif

#ifndef BUILD_READONLY

static bool _flush_nodes(Section *sect) {
    Custom *custom = (Custom*)sect->custom;

    trace("%d nodes in cache need to write", custom->_p_node_count);

    int i;
    for (i = 0; i < custom->_p_node_count; i++) {
        _write_node(sect, custom->_p_node_caches[i]);
//        _free_node(node);
    }

//    custom->_p_node_count = 0;
//    memset(custom->_p_node_caches, 0, _NODE_MAX_CACHE);

    return true;
}

#endif

#ifndef BUILD_READONLY

/**
 * 写入结点
 *
 * @param array $node  结点
 */
static int _write_node(Section *sect, PacketNode *p_node) {
    assert(_is_overflowed(p_node) == false);

    trace("node_offset = 0x%llX, parent_offset = 0x%llX", p_node->offset_self, p_node->offset_parent);

    memcpy(p_node->node->blob, p_node->blob_ex, (size_t)(ELEMENT_SIZE * p_node->node->numElement));

    void *src_ptr_elem = _ext_elem_ptr(p_node, 0);
    void *dst_ptr_elem = _std_elem_ptr(p_node, 0);
    int dst_distance = 0;
    int i;
    for (i = 0; i < p_node->node->numElement; i++) {
        int src_distance = _com_elem_key_str_distance(src_ptr_elem);
        void *src_ptr_key_str = _ext_elem_key_str_ptr(p_node, src_distance);
        int src_len_key_str = _com_elem_key_str_len(src_ptr_key_str);

        dst_distance += src_len_key_str + 1;
        void *dst_ptr_key_str = _std_elem_key_str_ptr(p_node, dst_distance);
        memcpy(dst_ptr_key_str, src_ptr_key_str, (size_t)(src_len_key_str + 1));

        *((uint8_t *)(dst_ptr_elem + ELEMENT_KEY_OFFSET)) = (uint8_t)dst_distance;

        src_ptr_elem += ELEMENT_SIZE;
        dst_ptr_elem += ELEMENT_SIZE;
    }

    section_seek(sect, p_node->offset_self, SEEK_SET, _RELATIVE);
    struct_write_node(sect->_stream, p_node->node);

    return RETURN_CODE(WPDP_OK);
}

#endif

#ifndef BUILD_READONLY

/**
 * 将元素附加到结点结尾
 *
 * @param array   $node   结点
 * @param string  $key    元素的键
 * @param integer $value  元素的值
 *
 * @return bool 总是 true
 */
static bool _append_element(PacketNode *p_node, WPDP_String *key, int64_t value) {
    trace("node = 0x%llX, key = %s, value = 0x%llX", p_node->offset_self, key->str, value);

    if (p_node->node->isLeaf || p_node->node->ofsExtra != 0) {
        // 是叶子结点，或非空的普通结点
        void *ptr_last_elem = _ext_elem_ptr(p_node, p_node->node->numElement - 1);
        int distance_cur_furthest = p_node->distance_furthest_key;

        void *ptr_new_elem = ptr_last_elem + ELEMENT_SIZE;
        int distance_new_key = distance_cur_furthest + key->len + 1;
        void *ptr_new_key = _ext_elem_key_str_ptr(p_node, distance_new_key);

        uint8_t len_new_key = key->len;
        memcpy(ptr_new_key, &len_new_key, sizeof(len_new_key));
        memcpy(ptr_new_key + 1, key->str, (size_t)key->len);

        uint16_t distance = distance_new_key;
        memcpy(ptr_new_elem, &distance, sizeof(distance));
        memcpy(ptr_new_elem + sizeof(distance), &value, sizeof(value));

        p_node->distance_furthest_key = distance_new_key;
        p_node->node->numElement++;
    } else {
        // 是空的普通结点
        p_node->node->ofsExtra = value;
    }

    trace("node calculated size: %d, %s", _compute_node_size(p_node),
        (_is_overflowed(p_node) ? ", overflowed" : ""));

//    assert($node['_size'] == self::_computeNodeSize($node));

    return true;
}

#endif

#ifndef BUILD_READONLY

/**
 * 将元素插入到结点中的指定位置的元素后
 *
 * 当 $pos 为 -1 时将元素插入到最前面，为 0 时插入到 elements[0] 后，
 * 为 1 时插入到 elements[1] 后，为 n 时调用 _appendElement()
 * 方法将元素附加到结点结尾。其中 n = count(elements) - 1.
 *
 * @param array   $node   结点
 * @param string  $key    元素的键
 * @param integer $value  元素的值
 * @param integer $pos    定位元素的位置
 *
 * @return bool 总是 true
 */
static bool _insert_element_after(PacketNode *p_node, WPDP_String *key, int64_t value, int pos) {
    assert(pos >= -1);

    trace("node = 0x%llX, key = %s, value = 0x%llX, after pos %d", p_node->offset_self, key, value, pos);

    int count = p_node->node->numElement;

    if (pos == count - 1) {
        return _append_element(p_node, key, value);
    }

    void *ptr_last_elem = _ext_elem_ptr(p_node, p_node->node->numElement - 1);
    int distance_cur_furthest = p_node->distance_furthest_key;
    int distance_new_key = distance_cur_furthest + key->len + 1;
    void *ptr_new_key = _ext_elem_key_str_ptr(p_node, distance_new_key);

    uint8_t len_new_key = key->len;
    memcpy(ptr_new_key, &len_new_key, sizeof(len_new_key));
    memcpy(ptr_new_key + 1, key->str, (size_t)key->len);

    void *ptr_new_elem = _ext_elem_ptr(p_node, pos + 1);
    memmove(ptr_new_elem + ELEMENT_SIZE, ptr_new_elem,
        (size_t)(ELEMENT_SIZE * (p_node->node->numElement - (pos + 1))));

    uint16_t distance = distance_new_key;
    memcpy(ptr_new_elem, &distance, sizeof(distance));
    memcpy(ptr_new_elem + sizeof(distance), &value, sizeof(value));

    p_node->distance_furthest_key = distance_new_key;
    p_node->node->numElement++;

    trace("node calculated size: %d, %s", _compute_node_size(p_node),
        (_is_overflowed(p_node) ? ", overflowed" : ""));

//    assert('$node[\'_size\'] == self::_computeNodeSize($node)');

    return true;
}

#endif

#ifndef BUILD_READONLY

/**
 * 判断指定结点中的元素是否已溢出
 *
 * @param array $node  结点
 *
 * @return bool 若已溢出，返回 true，否则返回 false
 */
static bool _is_overflowed(PacketNode *p_node) {
    return (_compute_node_size(p_node) > NODE_DATA_SIZE);
}

#endif

#ifndef BUILD_READONLY

/**
 * 计算结点中所有元素的键所占空间的字节数
 *
 * @param array $node  结点
 *
 * @return integer 所占空间的字节数
 */
static int _compute_node_size(PacketNode *p_node) {
    if (p_node->node->numElement == 0) {
        return 0;
    }

    void *ptr_last_elem = _ext_elem_ptr(p_node, p_node->node->numElement - 1);
    int distance_last_key = _com_elem_key_str_distance(ptr_last_elem);

    int node_size = (ELEMENT_SIZE * (int)p_node->node->numElement) + distance_last_key;

    return node_size;
}

#endif

#ifndef BUILD_READONLY

/**
 * 计算元素的键所占空间的字节数
 *
 * @param string $key   元素的键
 *
 * @return integer 所占空间的字节数
 */
static int _compute_element_size(WPDP_String *key) {
//          ptr + ofs + key_len + key
    return (2 + 8 + 1 + key->len);
}

#endif

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

#ifndef BUILD_READONLY

/**
 * 查找指定键在结点中的最右元素的位置
 *
 *       [0][1][2][3][4][5][6]
 * keys = 2, 3, 3, 5, 7, 7, 8
 *
 * +---------+----------+--------------+
 * | desired | found at | insert after |
 * +---------+----------+--------------+
 * |       1 |        / |           -1 |
 * |       2 |        0 |            0 |
 * |       3 |        2 |            2 |
 * |       4 |        / |            2 |
 * |       5 |        3 |            3 |
 * |       6 |        / |            3 |
 * |       7 |        5 |            5 |
 * |       8 |        6 |            6 |
 * |       9 |        / |            6 |
 * +---------+----------+--------------+
 *
 * 由 _binarySearchLeftmost() 方法修改而来
 *
 * @param array  $node        结点
 * @param string $desired     要查找的键
 * @param bool   $for_insert  是否用于插入元素目的
 *
 * @return integer 位置
 */
static int _binary_search_rightmost(PacketNode *p_node, WPDP_String *desired, bool for_insert) {
    trace("desired = %s, %s", desired->str, (for_insert ? ", for insert" : ""));

    int count = p_node->node->numElement;

    if (count == 0 || _key_compare(p_node, count - 1, desired) < 0) {
        trace("out of right bound");
        return (for_insert ? (count - 1) : _BINARY_SEARCH_NOT_FOUND);
    } else if (_key_compare(p_node, 0, desired) > 0) {
        trace("out of left bound");
        return (for_insert ? -1 : _BINARY_SEARCH_NOT_FOUND);
    }

    int m = (int)ceil(log2(count));
    int probe = (int)(count - pow(2, m - 1));
    int diff = (int)pow(2, m - 2);

    while (diff > 0) {
        trace("probe = %d (diff = $d)", probe, diff);
        if (probe >= 0 && _key_compare(p_node, probe, desired) > 0) {
            probe -= diff;
        } else {
            probe += diff;
        }
        // diff 为正数，不必再加 floor()
        diff = (int)(diff / 2);
    }

    trace("probe = %d (diff = %d)", probe, diff);

    if (probe >= 0 && _key_compare(p_node, probe, desired) == 0) {
        return probe;
    } else if (probe - 1 >= 0 && _key_compare(p_node, probe - 1, desired) == 0) {
        return probe - 1;
    } else if (for_insert && probe >= 0 && _key_compare(p_node, probe, desired) < 0) {
        return probe;
    } else if (for_insert && probe - 1 >= 0 && _key_compare(p_node, probe - 1, desired) < 0) {
        return probe - 1;
    } else {
        return _BINARY_SEARCH_NOT_FOUND;
    }
}

#endif

/**
 * 比较结点中指定下标元素的键与另一个给定键的大小
 *
 * @param array   $node   结点
 * @param integer $index  key1 在结点元素数组中的下标
 * @param string  $key    key2
 *
 * @return integer 如果 key1 小于 key2，返回 < 0 的值，大于则返回 > 0 的值，
 *                 等于则返回 0
 */
static int _key_compare(PacketNode *p_node, int index, WPDP_String *key) {
//    assert('array_key_exists($index, $node[\'elements\'])');

    WPDP_String *key_in_elem = _get_element_key(p_node, index);

    int retval = wpdp_string_compare(key_in_elem, key);

    return retval;
}

static WPDP_String *_get_element_key(PacketNode *p_node, int index) {
    void *ptr_elem = _ext_elem_ptr(p_node, index);
    void *ptr_key = _ext_elem_key_str_ptr(p_node, _com_elem_key_str_distance(ptr_elem));
    int len_key = _com_elem_key_str_len(ptr_key);

    return wpdp_string_direct(ptr_key + 1, len_key);
}

static int64_t _get_element_value(PacketNode *p_node, int index) {
    void *ptr_elem = _ext_elem_ptr(p_node, index);

    return *((int64_t *)(ptr_elem + ELEMENT_VALUE_OFFSET));
}
