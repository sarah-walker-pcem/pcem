#ifndef __HAVE_RAMDISK_H__
#define __HAVE_RAMDISK_H__
/*
 * Growable RAM disk memory stream for PCem by MMaster (2024)
 *
 * - Automatically grows memory buffer (disk size)
 * - Maintains expected size separate from allocated size
 * - Only writes and set_size calls grow memory
 * - Allows preload from raw image file
 * - Behaves similarly to fread, fwrite & fseek
 *
 * Note: Maximum size is 2GB - 1B
 */

/* ramdisk context */
typedef struct ramdisk ramdisk_t;

/**
 * Create new ramdisk.
 */
ramdisk_t *ramdisk_init();

/**
 * Release ramdisk memory.
 *
 * @param ramdisk Ramdisk context
 */
void ramdisk_free(ramdisk_t *ramdisk);

/**
 * Set required size of the disk.
 *
 * @note Disk is still growable with writes beyond end.
 * @param ramdisk Ramdisk context
 * @param size New size of the disk
 * @raturn 0 on success, -1 if unable to resize
 */
int ramdisk_set_size(ramdisk_t *ramdisk, size_t size);

/**
 * Write data to ramdisk at cursor.
 *
 * @note Data may be written beyond the current size of the disk.
 * @param ramdisk Ramdisk context
 * @param buf Buffer containing data to be written
 * @param size Size of the buffer
 * @return 0 on EOF, -1 on error (sets errno), >0 number of bytes written
 */
int ramdisk_write(ramdisk_t *ramdisk, const char *buf, size_t size);

/**
 * Read data from ramdisk at cursor.
 *
 * @param ramdisk Ramdisk context
 * @param buf Buffer for data to be read to
 * @param size Size of the buffer
 * @return 0 on EOF, -1 on error (sets errno), >0 number of bytes read
 */
int ramdisk_read(ramdisk_t *ramdisk, char *buf, size_t size);

/**
 * Seek within the ramdisk. Moves the cursor.
 *
 * @param ramdisk Ramdisk context
 * @param offset Offset from position specified by "whence" parameter.
 * @param whence SEEK_SET, SEEK_CUR or SEEK_END
 * @return >=0 disk cursor offset, -1 on error (sets errno)
 */
int ramdisk_seek(ramdisk_t *ramdisk, off_t offset, int whence);

/**
 * Get temporary pointer to buffer from current cursor in ramdisk.
 * 
 * Warning: This pointer is volatile and may be invalidated by write and set size calls.
 * Do not try to free this buffer!
 *
 * @param ramdisk Ramdisk context
 * @param mem Pointer to memory pointer that will be set to ramdisk buffer.
 * @param size Pointer to size of the buffer
 * @return 0 on EOF, -1 on error (cursor outside of allocated memory)
 */
int ramdisk_get_cursor_mem(ramdisk_t *ramdisk, char **mem, size_t *size);

/**
 * Load data from file to ramdisk at cursor.
 *
 * @note Data may be written beyond the current size of the disk.
 * @param ramdisk Ramdisk context
 * @param fp raw image FILE pointer
 * @return 0 on EOF, -1 on error (sets errno), >0 number of bytes written
 */
int ramdisk_load_file(ramdisk_t *ramdisk, FILE *fp);

#endif /* !__HAVE_RAMDISK_H__ */

