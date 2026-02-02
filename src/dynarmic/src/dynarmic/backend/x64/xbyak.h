#pragma once

#include <unordered_map>
#include <unordered_set>

#ifdef __OPENORBIS__
#define XBYAK_NO_EXCEPTION 1
#endif
#define XBYAK_STD_UNORDERED_SET std::unordered_set
#define XBYAK_STD_UNORDERED_MAP std::unordered_map
#define XBYAK_STD_UNORDERED_MULTIMAP std::unordered_multimap

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>
