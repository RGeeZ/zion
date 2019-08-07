#pragma once

#include <map>
#include <memory>
#include <string>

#include "scheme.h"

extern const char *MAYBE_TYPE;
extern const char *INT_TYPE;
extern const char *UINT_TYPE;
extern const char *INT64_TYPE;
extern const char *UINT64_TYPE;
extern const char *INT32_TYPE;
extern const char *UINT32_TYPE;
extern const char *INT16_TYPE;
extern const char *UINT16_TYPE;
extern const char *INT8_TYPE;
extern const char *UINT8_TYPE;
extern const char *CHAR_TYPE;
extern const char *BOOL_TYPE;
extern const char *FLOAT_TYPE;
extern const char *MBS_TYPE;
extern const char *PTR_TO_MBS_TYPE;
extern const char *TYPEID_TYPE;
extern const char *ARROW_TYPE_OPERATOR;
extern const char *PTR_TYPE_OPERATOR;
extern const char *REF_TYPE_OPERATOR;
extern const char *VECTOR_TYPE;
extern const char *MAP_TYPE;
extern const char *SET_TYPE;
extern const char *STRING_TYPE;

const types::SchemeMap &get_builtins();
