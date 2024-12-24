#pragma once

// glm defines
// putting them here
// because idk where els to put hthem

#define GLM_FORCE_ALIGNED_GENTYPES
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

// unsigned ints
typedef unsigned char      n8;
typedef unsigned short     n16;
typedef unsigned int       n32;
typedef unsigned long long n64;

// signed ints
typedef signed char      s8;
typedef signed short     s16;
typedef signed int       s32;
typedef signed long long s64;

// floats
typedef float  f32;
typedef double f64;

// bool
typedef int  b32;
typedef char b8;

#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

STATIC_ASSERT(sizeof(n8) == 1, "Expectex u8 to be 1 byte.");
STATIC_ASSERT(sizeof(n16) == 2, "Expectex u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(n32) == 4, "Expectex u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(n64) == 8, "Expectex u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(s8) == 1, "Expectex i8 to be 1 byte.");
STATIC_ASSERT(sizeof(s16) == 2, "Expectex i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(s32) == 4, "Expectex i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(s64) == 8, "Expectex i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expectex f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expectex f64 to be 8 bytes.");

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
