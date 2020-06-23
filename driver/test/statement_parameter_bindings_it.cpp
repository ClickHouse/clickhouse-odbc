#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

class StatementParameterBindingsTest
    : public ClientTestBase
{
};

class StatementParameterArrayBindingsTest
    : public StatementParameterBindingsTest
    , public ::testing::WithParamInterface<std::size_t>
{
};

