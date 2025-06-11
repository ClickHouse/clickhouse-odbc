#pragma once

#include "driver/platform/platform.h"

#include <folly/memory/UninitializedMemoryHacks.h>
#define resize_without_initialization(container, size) folly::resizeWithoutInitialization(container, size)

FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char16_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char32_t)
