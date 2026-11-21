#include "file_io.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

uint32_t hu_crc32(const uint8_t* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return c ^ 0xFFFFFFFFu;
}

void hu_put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void hu_put32(uint8_t* p, uint32_t v) {
    hu_put16(p, (uint16_t)v);
    hu_put16(p + 2, (uint16_t)(v >> 16));
}

uint16_t hu_get16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t hu_get32(const uint8_t* p) {
    return hu_get16(p) | ((uint32_t)hu_get16(p + 2) << 16);
}

static FILE* open_utf8(const char* path, const char* mode) {
#ifdef _WIN32
    wchar_t wpath[1024], wmode[8];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024) == 0 ||
        MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8) == 0) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
#else
    return fopen(path, mode);
#endif
}

static int replace_file(const char* from, const char* to) {
#ifdef _WIN32
    wchar_t wfrom[1024], wto[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, from, -1, wfrom, 1024) == 0 ||
        MultiByteToWideChar(CP_UTF8, 0, to, -1, wto, 1024) == 0) {
        return -1;
    }
    return MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

int hu_read_file(const char* path, uint8_t** data, size_t* size) {
    FILE* f = open_utf8(path, "rb");
    long n;
    *data = NULL;
    *size = 0;
    if (f == NULL) {
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *data = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
    if (*data == NULL || fread(*data, 1, (size_t)n, f) != (size_t)n) {
        free(*data);
        *data = NULL;
        fclose(f);
        return -1;
    }
    fclose(f);
    *size = (size_t)n;
    return 0;
}

int hu_write_file(const char* path, const uint8_t* data, size_t size) {
    size_t len = strlen(path);
    char* tmp = (char*)malloc(len + 5);
    FILE* f;
    bool ok;
    if (tmp == NULL) {
        return -1;
    }
    memcpy(tmp, path, len);
    memcpy(tmp + len, ".tmp", 5);
    f = open_utf8(tmp, "wb");
    ok = f != NULL && fwrite(data, 1, size, f) == size;
    if (f != NULL) {
        ok = (fflush(f) == 0) && ok;
        ok = (fclose(f) == 0) && ok;
    }
    if (!ok || replace_file(tmp, path) != 0) {
        remove(tmp);
        free(tmp);
        return -1;
    }
    free(tmp);
    return 0;
}
