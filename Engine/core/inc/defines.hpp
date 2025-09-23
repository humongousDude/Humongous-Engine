#pragma once
#include <cstdint>

// unsigned ints
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// signed ints
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

// floats
using f32 = float;
using f64 = double;

// bool
using b8 = bool;
using b32 = u32;

static_assert(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

static_assert(sizeof(s8) == 1, "Expected s8 to be 1 byte.");
static_assert(sizeof(s16) == 2, "Expected s16 to be 2 bytes.");
static_assert(sizeof(s32) == 4, "Expected s32 to be 4 bytes.");
static_assert(sizeof(s64) == 8, "Expected s64 to be 8 bytes.");

static_assert(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
static_assert(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

/* #ifdef HEXPORT
// exports
#ifdef _MSC_VER
#define HAPI __declspec(dllexport)
#else
#define HAPI __attribute__((visibility("default")))
#endif
#else
// Imports
#ifdef _MSC_VER
#define HAPI __declspec(dllimport)
#else
#define KAPI
#endif
#endif */
