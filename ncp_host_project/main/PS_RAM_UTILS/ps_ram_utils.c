#include "ps_ram_utils.h"

#include "string.h"

char* psram_strdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char* dst = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (dst) {
        memcpy(dst, src, len);
    }
    return dst;
}