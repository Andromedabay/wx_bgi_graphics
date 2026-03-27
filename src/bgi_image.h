#pragma once

#include "bgi_types.h"

namespace bgi
{

    unsigned imageSizeForRect(int left, int top, int right, int bottom);
    bool captureImage(int left, int top, int right, int bottom, void *bitmap);
    bool drawImage(int left, int top, const void *bitmap, int op);

} // namespace bgi