#pragma once

#include "mesh485/types.h"

bool mesh485_parse_request(const char* json, Mesh485Request* out);
size_t mesh485_write_response(char* buf, size_t max, const Mesh485Response& resp);
