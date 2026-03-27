#include "driver/utils/type_info.h"

#include <gtest/gtest.h>

TEST(ConvertSQLOrCTypeToDataSourceType, CTypeDateMapsToDate32) {
    BoundTypeInfo ti;
    ti.c_type = SQL_C_TYPE_DATE;
    ASSERT_EQ(convertCTypeToDataSourceType(ti), "Date32");
}

TEST(ConvertSQLOrCTypeToDataSourceType, SQLTypeDateMapsToDate32) {
    BoundTypeInfo ti;
    ti.sql_type = SQL_TYPE_DATE;
    ASSERT_EQ(convertSQLOrCTypeToDataSourceType(ti), "Date32");
}

TEST(ConvertSQLOrCTypeToDataSourceType, NullableTypeDateMapsToNullableDate32) {
    BoundTypeInfo ti;
    ti.sql_type = SQL_TYPE_DATE;
    ti.is_nullable = true;
    ASSERT_EQ(convertSQLOrCTypeToDataSourceType(ti), "Nullable(Date32)");
}
