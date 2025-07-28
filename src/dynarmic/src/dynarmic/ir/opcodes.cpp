/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/ir/opcodes.h"

#include <array>
#include <vector>

#include "dynarmic/ir/type.h"

namespace Dynarmic::IR {

// Opcode information

namespace OpcodeInfo {

constexpr Type Void = Type::Void;
constexpr Type A32Reg = Type::A32Reg;
constexpr Type A32ExtReg = Type::A32ExtReg;
constexpr Type A64Reg = Type::A64Reg;
constexpr Type A64Vec = Type::A64Vec;
constexpr Type Opaque = Type::Opaque;
constexpr Type U1 = Type::U1;
constexpr Type U8 = Type::U8;
constexpr Type U16 = Type::U16;
constexpr Type U32 = Type::U32;
constexpr Type U64 = Type::U64;
constexpr Type U128 = Type::U128;
constexpr Type CoprocInfo = Type::CoprocInfo;
constexpr Type NZCV = Type::NZCVFlags;
constexpr Type Cond = Type::Cond;
constexpr Type Table = Type::Table;
constexpr Type AccType = Type::AccType;

struct Meta {
    std::array<Type, 4> arg_types;
    Type type;
    uint8_t count;
};

// Evil macro magic for Intel C++ compiler
#define PP_ARG_N( \
    _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, N, ...) N
#define PP_RSEQ_N() \
    63, 62, 61, 60,                                   \
    59, 58, 57, 56, 55, 54, 53, 52, 51, 50,           \
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40,           \
    39, 38, 37, 36, 35, 34, 33, 32, 31, 30,           \
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20,           \
    19, 18, 17, 16, 15, 14, 13, 12, 11, 10,           \
    9,  8,  7,  6,  5,  4,  3,  2,  1,  0
#define PP_NARG_(...)    PP_ARG_N(__VA_ARGS__)    
#define PP_NARG(...) (sizeof(#__VA_ARGS__) - 1 ? PP_NARG_(__VA_ARGS__, PP_RSEQ_N()) : 0)

alignas(64) static const Meta opcode_info[] = {
#define OPCODE(name, type, ...) Meta{{__VA_ARGS__}, type, PP_NARG(__VA_ARGS__)},
#define A32OPC(name, type, ...) Meta{{__VA_ARGS__}, type, PP_NARG(__VA_ARGS__)},
#define A64OPC(name, type, ...) Meta{{__VA_ARGS__}, type, PP_NARG(__VA_ARGS__)},
#include "./opcodes.inc"
#undef OPCODE
#undef A32OPC
#undef A64OPC
};

}  // namespace OpcodeInfo

/// @brief Get return type of an opcode
Type GetTypeOf(Opcode op) noexcept {
    return OpcodeInfo::opcode_info[size_t(op)].type;
}

/// @brief Get the number of arguments an opcode accepts
size_t GetNumArgsOf(Opcode op) noexcept {
    return OpcodeInfo::opcode_info[size_t(op)].count;
}

/// @brief Get the required type of an argument of an opcode
Type GetArgTypeOf(Opcode op, size_t arg_index) noexcept {
    return OpcodeInfo::opcode_info[size_t(op)].arg_types[arg_index];
}

/// @brief Get the name of an opcode.
std::string_view GetNameOf(Opcode op) noexcept {
    static const std::string_view opcode_names[] = {
#define OPCODE(name, type, ...) #name,
#define A32OPC(name, type, ...) #name,
#define A64OPC(name, type, ...) #name,
#include "./opcodes.inc"
#undef OPCODE
#undef A32OPC
#undef A64OPC
    };
    return opcode_names[size_t(op)];
}

}  // namespace Dynarmic::IR
