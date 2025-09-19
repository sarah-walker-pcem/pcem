/*
 * Growable RAM disk memory stream for PCem by MMaster (2024)
 * Inspired by fmem https://github.com/Snaipe/fmem
 */
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define KB_SIZE * 1024
#define MB_SIZE * (1024 KB_SIZE)
#define GB_SIZE * (1024 MB_SIZE)

// Max: 2 GB - 1B regardless of 32 / 64 bit
#define MAX_STREAM_SIZE (-1 + 1 GB_SIZE + 1 GB_SIZE)

/* Stream section */
typedef struct ramdisk_stream {
        uint8_t *mem;
        size_t _size; // internal allocation size
        size_t size;  // expected size
        size_t cursor;
} ramdisk_stream_t;

ramdisk_stream_t *ramdisk_stream_init() {
        ramdisk_stream_t *stream = (ramdisk_stream_t *)malloc(sizeof(ramdisk_stream_t));
        if (stream == NULL)
                return NULL;

        memset(stream, 0, sizeof(*stream));

        stream->_size = 4 KB_SIZE;
        stream->mem = (uint8_t *)malloc(stream->_size);
        if (stream->mem == NULL) {
                free(stream);
                return NULL;
        }
        return stream;
}

void ramdisk_stream_free(ramdisk_stream_t *stream) {
        free(stream->mem);
        free(stream);
}

size_t golden_growth_ceil(size_t n)
{
        /* This effectively is a return ceil(n * φ).
           φ is approximatively 207 / (2^7), so we shift our result by
           6, then perform our ceil by adding the remainder of the last division
           by 2 of the result to itself. */

        n = (n * 207) >> 6;
        n = (n >> 1) + (n & 1);
        return n;
}

int ramdisk_stream_grow(ramdisk_stream_t *stream, size_t required) {
        if (stream->cursor > MAX_STREAM_SIZE - required) {
                errno = EOVERFLOW;
                return -1;
        }
        required += stream->cursor;

        size_t newsize = stream->_size;
        if (required <= newsize)
                return 0;

        while (required > newsize) {
                newsize = golden_growth_ceil(newsize);
        }

        if (newsize > MAX_STREAM_SIZE)
                newsize = MAX_STREAM_SIZE;

        uint8_t *newmem = realloc(stream->mem, newsize);
        if (newmem == NULL && newsize > required)
                newmem = realloc(stream->mem, required);
        if (newmem == NULL) {
                errno = ENOMEM;
                return -1;
        }

        stream->mem = newmem;
        stream->_size = newsize;
        return 0;
}

int ramdisk_stream_resize(ramdisk_stream_t *stream, size_t size) {
        if (size <= stream->_size)
                return 0;

        if (size > MAX_STREAM_SIZE) {
                errno = EOVERFLOW;
                return -1;
        }

        uint8_t *newmem = realloc(stream->mem, size);
        if (newmem == NULL) {
                errno = ENOMEM;
                return -1;
        }

        stream->mem = newmem;
        stream->_size = size;
        return 0;
}

// get buffer for reading (not incl. allocated space outside of current size)
int ramdisk_stream_cursor_buf_r(ramdisk_stream_t *stream, uint8_t **buf, size_t *buf_size) {
        if (stream->size < stream->cursor)
                return -1;
        *buf_size = stream->size - stream->cursor;
        *buf = stream->mem + stream->cursor;
        return 0;
}

// get buffer for writing (incl. allocated space outside of current size)
int ramdisk_stream_cursor_buf_w(ramdisk_stream_t *stream, uint8_t **buf, size_t *buf_size) {
        if (stream->_size < stream->cursor)
                return -1;
        *buf_size = stream->_size - stream->cursor;
        *buf = stream->mem + stream->cursor;
        return 0;
}

size_t ramdisk_stream_copy_buf(uint8_t *dst, size_t dst_size, const uint8_t *src, size_t src_size) {
        size_t len = src_size < dst_size ? src_size : dst_size;
        memcpy(dst, src, len);
        return len;
}

/* Disk section */

typedef struct ramdisk {
        ramdisk_stream_t *stream;
} ramdisk_t;

ramdisk_t *ramdisk_init() {
        ramdisk_t *ramdisk = (ramdisk_t *)malloc(sizeof(ramdisk_t));
        if (ramdisk == NULL)
                return NULL;

        ramdisk->stream = ramdisk_stream_init();
        if (ramdisk->stream == NULL) {
                free(ramdisk);
                return NULL;
        }
        return ramdisk;
}

void ramdisk_free(ramdisk_t *ramdisk) {
        ramdisk_stream_free(ramdisk->stream);
        free(ramdisk);
}

int ramdisk_set_size(ramdisk_t *ramdisk, size_t size) {
        if (size > ramdisk->stream->_size) {
                if (ramdisk_stream_resize(ramdisk->stream, size) < 0)
                        return -1;
        }
        ramdisk->stream->size = size;
        return 0;
}

int ramdisk_write(ramdisk_t *ramdisk, const char *buf, size_t size) {
        if (ramdisk->stream->cursor > MAX_STREAM_SIZE - size)
                size = MAX_STREAM_SIZE - ramdisk->stream->cursor;

        char *dst_buf;
        size_t dst_buf_size;
        if (ramdisk_stream_grow(ramdisk->stream, size) < 0)
                return -1;

        if (ramdisk_stream_cursor_buf_w(ramdisk->stream, (uint8_t **)&dst_buf, &dst_buf_size) < 0)
                return 0;

        size_t written = ramdisk_stream_copy_buf(dst_buf, dst_buf_size, buf, size);
        if (written > INT_MAX) {
                errno = EOVERFLOW;
                return -1;
        }
        ramdisk->stream->cursor += written;

        // if it was written beyond current 'end of file' move the EOF
        if (ramdisk->stream->size < ramdisk->stream->cursor)
                ramdisk->stream->size = ramdisk->stream->cursor;

        return written;
}

int ramdisk_read(ramdisk_t *ramdisk, char *buf, size_t size) {
        if (ramdisk->stream->cursor > MAX_STREAM_SIZE - size)
                size = MAX_STREAM_SIZE - ramdisk->stream->cursor;

        // end of file at MAX_STREAM_SIZE
        if (size == 0)
                return 0;

        char *src_buf;
        size_t src_buf_size;
        size_t written = 0;
        if (ramdisk_stream_cursor_buf_r(ramdisk->stream, (uint8_t **)&src_buf, &src_buf_size) == 0) {
                if (src_buf_size == 0)
                        return 0; // eof

                written = ramdisk_stream_copy_buf(buf, size, src_buf, src_buf_size);
        }

        if (written > INT_MAX) {
                errno = EOVERFLOW;
                return -1;
        }

        ramdisk->stream->cursor += written;
        return written;
}

int ramdisk_seek(ramdisk_t *ramdisk, off_t offset, int whence) {
        size_t new_offset;
        switch (whence) {
                case SEEK_SET:
                        new_offset = offset;
                        break;
                case SEEK_CUR:
                        new_offset = ramdisk->stream->cursor + offset;
                        break;
                case SEEK_END:
                        new_offset = ramdisk->stream->size + offset;
                        break;
                default:
                        errno = EINVAL;
                        return -1;
        }

        if (new_offset < 0 || new_offset > MAX_STREAM_SIZE) {
                errno = EOVERFLOW;
                return -1;
        }

        ramdisk->stream->cursor = new_offset;
        return new_offset;
}

int ramdisk_get_cursor_mem(ramdisk_t *ramdisk, char **mem, size_t *size) {
        return ramdisk_stream_cursor_buf_w(ramdisk->stream, (uint8_t **)mem, size);
}

int ramdisk_load_file(ramdisk_t *ramdisk, FILE *fp) {
        // get file size
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (ramdisk_stream_resize(ramdisk->stream, size) < 0)
                return -1;

        ramdisk_seek(ramdisk, 0, SEEK_SET);

        char *buf;
        size_t buf_size;
        if (ramdisk_stream_cursor_buf_w(ramdisk->stream, (uint8_t **)&buf, &buf_size) < 0)
                return -1;

        size_t len = fread(buf, 1, buf_size, fp);
        if (len > 0)
                ramdisk->stream->size = ramdisk->stream->cursor + len;

        return len;
}

