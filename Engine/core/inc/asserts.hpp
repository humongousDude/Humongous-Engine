#pragma once

#include "defines.hpp"

#ifdef HGASSERTIONS_ENABLED
#if _MSV_VER
#include <intrin.h>
#define BreakDebug() __debugbreak()
#else
#define BreakDebug() __builtin_trap()
#endif

void ReportAssertionFaliure(const char* expression, const char* message, const char* file, s32 line);

#define HGASSERT(expr)                                                                                                                             \
    {                                                                                                                                              \
        if(expr) {}                                                                                                                                \
        else                                                                                                                                       \
        {                                                                                                                                          \
            ReportAssertionFaliure(#expr, "", __FILE__, __LINE__);                                                                                 \
            BreakDebug();                                                                                                                          \
        }                                                                                                                                          \
    }

#define HGASSERT_MSG(expr, message)                                                                                                                \
    {                                                                                                                                              \
        if(expr) {}                                                                                                                                \
        else                                                                                                                                       \
        {                                                                                                                                          \
            ReportAssertionFaliure(#expr, message, __FILE__, __LINE__);                                                                            \
            BreakDebug();                                                                                                                          \
        }                                                                                                                                          \
    }

#ifndef HGRELEASE
#define HGASSERT_DEBUG(expr)                                                                                                                       \
    {                                                                                                                                              \
        if(expr) {}                                                                                                                                \
        else                                                                                                                                       \
        {                                                                                                                                          \
            ReportAssertionFaliure(#expr, "", __FILE__, __LINE__);                                                                                 \
            BreakDebug();                                                                                                                          \
        }                                                                                                                                          \
    }
#else
#define HGASSERT_DEBUG(expr)
#endif

#else
#define HGASSERT(expr)
#define HGASSERT_MSG(expr, message)
#define HGASSERT_DEBUG(expr)
#endif
