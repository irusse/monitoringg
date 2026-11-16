#pragma once

#include <stddef.h>
#include <stdint.h>

size_t mesh485_b64_encode(const uint8_t* src, size_t len, char* dst, size_t dst_max);
size_t mesh485_b64_decode(const char* src, uint8_t* dst, size_t dst_max);
