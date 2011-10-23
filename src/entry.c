#include "internal.h"

#define DEFAULT_CAPACITY    10

static int _ensure_capacity(WPDP_Entry_Attributes *attrs, int min);

WPDP_Entry_Attributes *wpdp_entry_attributes_create(void) {
    WPDP_Entry_Attributes *attrs;

    attrs = wpdp_new_zero(WPDP_Entry_Attributes, 1);

    attrs->capacity = DEFAULT_CAPACITY;
    attrs->size = 0;
    attrs->attrs = wpdp_new_zero(WPDP_Entry_Attribute *, attrs->capacity);

    return attrs;
}

int wpdp_entry_attributes_add(WPDP_Entry_Attributes *attrs, WPDP_Entry_Attribute *attr) {
    if (attrs->size == attrs->capacity) {
        _ensure_capacity(attrs, attrs->size + 1);
    }

    attrs->attrs[attrs->size] = attr;
    attrs->size++;

    return WPDP_OK;
}

int wpdp_entry_attributes_free(WPDP_Entry_Attributes *attrs) {
    int i;

    for (i = 0; i < attrs->size; i++) {
        wpdp_string_free(attrs->attrs[i]->name);
        wpdp_string_free(attrs->attrs[i]->value);
        wpdp_free(attrs->attrs[i]);
    }

    wpdp_free(attrs->attrs);
    wpdp_free(attrs);

    return WPDP_OK;
}

WPDP_Entry_Attribute *wpdp_entry_attribute_create(WPDP_String *name, WPDP_String *value, bool index) {
    WPDP_Entry_Attribute *attr;

    attr = wpdp_new_zero(WPDP_Entry_Attribute, 1);
    if (attr == NULL) {
        return NULL;
    }

    attr->name = name;
    attr->value = value;
    attr->index = index;

    return attr;
}

static int _ensure_capacity(WPDP_Entry_Attributes *attrs, int min) {
    if (attrs->capacity < min) {
        int num = attrs->capacity * 2;
        if (num < min) {
            num = min;
        }
        attrs->capacity = num;
        attrs->attrs = wpdp_realloc(attrs->attrs, ((int)sizeof(WPDP_Entry_Attribute *) * attrs->capacity));
    }

    return WPDP_OK;
}











#define AUX_FP(stream) (((WPDP_EntryContentsStreamData*)((stream)->aux))->fp)

typedef struct _file_stream_data {
    WPDP                *_dp;
    WPDP_Entry_Args     *_args;
    int64_t             _offset;
} EntryContentsStreamData;

static size_t entry_contents_stream_read(WPIO_Stream *stream, void *buffer, size_t length) {
    EntryContentsStreamData *aux_data = (EntryContentsStreamData *)stream->aux;

    int64_t len_read = 0;
    section_contents_get_contents(aux_data->_dp->_contents, aux_data->_args, aux_data->_offset, length,
                                  buffer, &len_read);

    aux_data->_offset += len_read;

    return 0;
}

static size_t entry_contents_stream_write(WPIO_Stream *stream, const void *buffer, size_t length) {
    // to be noticed
}

static int entry_contents_stream_flush(WPIO_Stream *stream) {
    // to be noticed
}

static int entry_contents_stream_seek(WPIO_Stream *stream, off64_t offset, int whence) {
    EntryContentsStreamData *aux_data = (EntryContentsStreamData *)stream->aux;

    assert(IN_ARRAY_3(whence, SEEK_SET, SEEK_CUR, SEEK_END));

    if (whence == SEEK_SET) {
        aux_data->_offset = offset;
    } else if (whence == SEEK_END) {
        aux_data->_offset = aux_data->_args->entry_info.original_length + offset;
    } else if (whence == SEEK_CUR) {
        aux_data->_offset += offset;
    }

    return 0;
}

static off64_t entry_contents_stream_tell(WPIO_Stream *stream) {
    EntryContentsStreamData *aux_data = (EntryContentsStreamData *)stream->aux;

    return aux_data->_offset;
}

static int entry_contents_stream_eof(WPIO_Stream *stream) {
    EntryContentsStreamData *aux_data = (EntryContentsStreamData *)stream->aux;

    return (aux_data->_offset == aux_data->_args->entry_info.original_length);
}

static int entry_contents_stream_close(WPIO_Stream *stream) {
    // to be noticed
}

static const WPIO_StreamOps entry_contents_stream_ops = {
    entry_contents_stream_read,
    entry_contents_stream_write,
    entry_contents_stream_flush,
    entry_contents_stream_seek,
    entry_contents_stream_tell,
    entry_contents_stream_eof,
    entry_contents_stream_close
};
/*
WPIO_API WPIO_Stream *entry_contents_stream_open(const char *filename, const WPIO_Mode mode) {
    EntryContentsStreamData *aux_data;

    if (filename == NULL) {
        return NULL;
    }

    aux = malloc(sizeof(*aux));

    aux->fp = fopen64(filename, (mode == WPIO_MODE_READ_WRITE) ? "r+b" : "rb");
    if (aux->fp == NULL) {
        return NULL;
    }

    return wpio_alloc(&file_ops, aux);
}
*/
