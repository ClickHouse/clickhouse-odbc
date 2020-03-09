#pragma once

#include "driver/platform/platform.h"

#if defined(_MSC_VER) && _MSC_VER > 1916 // Not supported yet for Visual Studio 2019 and later.
#   define resize_without_initialization(container, size) container.resize(size)
#else
#   include <folly/memory/UninitializedMemoryHacks.h>
#   define resize_without_initialization(container, size) folly::resizeWithoutInitialization(container, size)
#endif
