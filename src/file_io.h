/* Internal: the file helpers the stores share (trainer memory, hue choices). Not installed with the public headers. */
#ifndef HIPPODROME_FILE_IO_H
#define HIPPODROME_FILE_IO_H

#include <stddef.h>
#include <stdint.h>

/* CRC-32 with the zlib polynomial. */
uint32_t hu_crc32(const uint8_t* p, size_t n);

/* Little-endian fields. */
void hu_put16(uint8_t* p, uint16_t v);
void hu_put32(uint8_t* p, uint32_t v);
uint16_t hu_get16(const uint8_t* p);
uint32_t hu_get32(const uint8_t* p);

/* Reads the whole file (path in UTF-8, on Windows too) into malloc'd memory the caller frees.
 * Returns 0 on success, 1 when there is no such file, -1 on a read error. */
int hu_read_file(const char* path, uint8_t** data, size_t* size);
/* Writes `data` to `path` through `<path>.tmp` and a rename, so a crash never leaves a half-written file.
 * Returns 0 on success, -1 on an error. */
int hu_write_file(const char* path, const uint8_t* data, size_t size);

#endif
