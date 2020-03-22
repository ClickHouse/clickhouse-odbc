#pragma once

#include "driver/platform/platform.h"

#include <folly/memory/UninitializedMemoryHacks.h>
#define resize_without_initialization(container, size) folly::resizeWithoutInitialization(container, size)
