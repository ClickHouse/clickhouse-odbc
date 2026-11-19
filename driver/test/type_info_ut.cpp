#include "driver/utils/type_info.h"
#include "driver/result_set.h"

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

TEST(ColumnInfo, LowCardinalityNullability) {
    for (const bool nullable : {false, true}) {
        const std::string type = nullable ? "LowCardinality(Nullable(String))" : "LowCardinality(String)";
        TypeAst ast;
        ASSERT_TRUE(TypeParser(type).parse(&ast));

        ColumnInfo info;
        info.assignTypeInfo(ast, "UTC");

        EXPECT_EQ(info.is_nullable, nullable) << type;
        EXPECT_EQ(info.type_without_parameters, "String") << type;
    }
}
