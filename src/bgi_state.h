#pragma once

#include <mutex>
#include "bgi_types.h"

namespace bgi
{

    // Global rendering mutex — all public API calls lock this before touching gState.
    extern std::mutex gMutex;

    // Single instance of all mutable rendering state.
    extern BgiState gState;

} // namespace bgi
