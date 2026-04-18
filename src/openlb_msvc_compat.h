#pragma once

#ifdef _MSC_VER

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
#endif

#endif
