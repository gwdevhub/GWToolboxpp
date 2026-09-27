#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t table1_first[14];
    uint16_t table1_index[14];
    uint8_t table2[256];
    uint8_t table3[32];
    uint8_t length_bits[29];
    uint8_t distance_bits[32];
    uint16_t distance_base[32];
} GwInflateTables;

#ifdef __cplusplus
extern "C" {
#endif
int gw_inflate_all(const uint8_t* source, size_t source_size, uint8_t* output,
                   size_t capacity, size_t* output_size, const GwInflateTables* tables);
#ifdef __cplusplus
}
#endif
