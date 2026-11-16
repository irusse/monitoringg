#include "mesh485/base64.h"

static const char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

size_t mesh485_b64_encode(const uint8_t* src, size_t len, char* dst, size_t dst_max) {
  if (!dst || dst_max == 0) return 0;
  dst[0] = 0;
  if (!src && len) return 0;
  const size_t need = ((len + 2) / 3) * 4;
  if (need + 1 > dst_max) return 0;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    const uint32_t n = (uint32_t)src[i] << 16
                     | (i + 1 < len ? (uint32_t)src[i + 1] << 8 : 0)
                     | (i + 2 < len ? (uint32_t)src[i + 2] : 0);
    dst[o++] = kB64[(n >> 18) & 63];
    dst[o++] = kB64[(n >> 12) & 63];
    dst[o++] = (i + 1 < len) ? kB64[(n >> 6) & 63] : '=';
    dst[o++] = (i + 2 < len) ? kB64[n & 63] : '=';
  }
  dst[o] = 0;
  return o;
}

size_t mesh485_b64_decode(const char* src, uint8_t* dst, size_t dst_max) {
  if (!src || !dst) return 0;
  size_t n = 0;
  int val[4];
  int k = 0;
  for (const char* p = src; *p; ++p) {
    if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') continue;
    if (*p == '=') {
      val[k++] = -2;
    } else {
      const int v = b64Val(*p);
      if (v < 0) return 0;
      val[k++] = v;
    }
    if (k == 4) {
      if (n >= dst_max) return 0;
      dst[n++] = (uint8_t)(((val[0] & 63) << 2) | ((val[1] & 63) >> 4));
      if (val[2] >= 0) {
        if (n >= dst_max) return 0;
        dst[n++] = (uint8_t)(((val[1] & 15) << 4) | ((val[2] & 63) >> 2));
      }
      if (val[3] >= 0) {
        if (n >= dst_max) return 0;
        dst[n++] = (uint8_t)(((val[2] & 3) << 6) | (val[3] & 63));
      }
      k = 0;
    }
  }
  if (k != 0) return 0;
  return n;
}
