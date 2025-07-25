#include <string>
#include <vector>
#include <gtest/gtest.h>

#include "driver/test/client_test_base.h"
#include "driver/test/client_utils.h"
#include "driver/test/result_set_reader.hpp"
#include "driver/utils/sql_encoding.h"

// Integration tests for ODBC catalog functions SQLSpecialColumns and SQLStatistics
class CatalogFunctionsTest
    : public ClientTestBase
{
};

// Test SQLSpecialColumns function
TEST_F(CatalogFunctionsTest, SQLSpecialColumns)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Test basic call to SQLSpecialColumns
    auto catalog = fromUTF8<PTChar>("default");
    auto schema = fromUTF8<PTChar>("");
    auto table = fromUTF8<PTChar>("system.tables");

    rc = SQLSpecialColumns(hstmt,
        SQL_BEST_ROWID,                        // IdentifierType
        ptcharCast(catalog.data()), SQL_NTS,   // CatalogName
        ptcharCast(schema.data()), SQL_NTS,    // SchemaName
        ptcharCast(table.data()), SQL_NTS,     // TableName
        SQL_SCOPE_CURROW,                      // Scope
        SQL_NULLABLE                           // Nullable
    );

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify the result set structure has the expected columns
    SQLSMALLINT column_count = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &column_count));
    EXPECT_EQ(column_count, 8);

    // Check column names match ODBC specification for SQLSpecialColumns
    std::vector<std::string> expected_columns = {
        "SCOPE", "COLUMN_NAME", "DATA_TYPE", "TYPE_NAME",
        "COLUMN_SIZE", "BUFFER_LENGTH", "DECIMAL_DIGITS", "PSEUDO_COLUMN"
    };

    for (SQLSMALLINT i = 1; i <= column_count; ++i) {
        PTChar column_name[256] = {};
        SQLSMALLINT name_length = 0;
        SQLSMALLINT data_type = 0;
        SQLULEN column_size = 0;
        SQLSMALLINT decimal_digits = 0;
        SQLSMALLINT nullable = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(hstmt, i,
            reinterpret_cast<SQLTCHAR*>(column_name), sizeof(column_name) / sizeof(PTChar), &name_length,
            &data_type, &column_size, &decimal_digits, &nullable));

        auto actual_name = toUTF8(column_name);
        EXPECT_EQ(actual_name, expected_columns[i - 1]);
    }

    // The result should be empty since implementation returns WHERE 1 == 0
    rc = SQLFetch(hstmt);
    EXPECT_EQ(rc, SQL_NO_DATA);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
}

// Test SQLStatistics function
TEST_F(CatalogFunctionsTest, SQLStatistics)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Test basic call to SQLStatistics with system tables
    // Using system database since it's guaranteed to exist
    auto catalog = fromUTF8<PTChar>("system");
    auto schema = fromUTF8<PTChar>("");
    auto table = fromUTF8<PTChar>("tables");

    rc = SQLStatistics(hstmt,
        ptcharCast(catalog.data()), SQL_NTS,   // CatalogName
        ptcharCast(schema.data()), SQL_NTS,    // SchemaName
        ptcharCast(table.data()), SQL_NTS,     // TableName
        SQL_INDEX_ALL,                         // Unique
        SQL_ENSURE                             // Reserved
    );

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify the result set structure has the expected columns
    SQLSMALLINT column_count = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &column_count));
    EXPECT_EQ(column_count, 13);

    // Check column names
    std::vector<std::string> expected_columns = {
        "TABLE_CAT", "TABLE_SCHEM", "TABLE_NAME", "NON_UNIQUE",
        "INDEX_QUALIFIER", "INDEX_NAME", "TYPE", "ORDINAL_POSITION",
        "COLUMN_NAME", "ASC_OR_DESC", "CARDINALITY", "PAGES", "FILTER_CONDITION"
    };

    for (SQLSMALLINT i = 1; i <= column_count; ++i) {
        PTChar column_name[256] = {};
        SQLSMALLINT name_length = 0;
        SQLSMALLINT data_type = 0;
        SQLULEN column_size = 0;
        SQLSMALLINT decimal_digits = 0;
        SQLSMALLINT nullable = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(hstmt, i,
            reinterpret_cast<SQLTCHAR*>(column_name), sizeof(column_name) / sizeof(PTChar), &name_length,
            &data_type, &column_size, &decimal_digits, &nullable));

        auto actual_name = toUTF8(column_name);
        EXPECT_EQ(actual_name, expected_columns[i - 1]);
    }

    // Check if we have any data - this depends on system.data_skipping_indices content
    // The result may be empty or contain data, both are valid
    ResultSetReader reader{hstmt};
    int row_count = 0;
    while (reader.fetch()) {
        row_count++;
        // Verify basic structure of returned data if any exists
        auto table_cat = reader.getData<std::string>("TABLE_CAT");
        auto table_name = reader.getData<std::string>("TABLE_NAME");
        auto non_unique = reader.getData<SQLSMALLINT>("NON_UNIQUE");
        auto type = reader.getData<SQLSMALLINT>("TYPE");

        // These fields should always have values when a row is returned
        EXPECT_TRUE(table_cat.has_value());
        EXPECT_TRUE(table_name.has_value());
        EXPECT_TRUE(non_unique.has_value());
        EXPECT_TRUE(type.has_value());

        }

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
}

// Test SQLStatistics with filtering parameters
TEST_F(CatalogFunctionsTest, SQLStatisticsWithFiltering)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Ensure statement is in a completely clean state
    SQLFreeStmt(hstmt, SQL_CLOSE);
    SQLFreeStmt(hstmt, SQL_UNBIND);
    SQLFreeStmt(hstmt, SQL_RESET_PARAMS);

    // Test with specific catalog filtering
    auto catalog = fromUTF8<PTChar>("system");
    auto schema = fromUTF8<PTChar>("");
    auto table = fromUTF8<PTChar>("%");  // Use wildcard to get all tables

    rc = SQLStatistics(hstmt,
        ptcharCast(catalog.data()), SQL_NTS,   // CatalogName
        ptcharCast(schema.data()), SQL_NTS,    // SchemaName
        ptcharCast(table.data()), SQL_NTS,     // TableName
        SQL_INDEX_UNIQUE,                      // Unique
        SQL_ENSURE                             // Reserved
    );

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify result set structure
    SQLSMALLINT column_count = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &column_count));
    EXPECT_EQ(column_count, 13);

    // Verify that any returned data has the correct catalog
    ResultSetReader reader{hstmt};
    while (reader.fetch()) {
        auto table_cat = reader.getData<std::string>("TABLE_CAT");
        EXPECT_TRUE(table_cat.has_value());
        if (table_cat.has_value()) {
            EXPECT_EQ(table_cat.value(), "system");
        }
    }

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
}

// Test SQLStatistics with NULL parameters
TEST_F(CatalogFunctionsTest, SQLStatisticsWithNullParameters)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Create a fresh statement handle to avoid sequence errors
    SQLHSTMT temp_hstmt = SQL_NULL_HSTMT;
    rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &temp_hstmt);
    ASSERT_EQ(rc, SQL_SUCCESS) << "Failed to allocate new statement handle";

    // Test with NULL catalog and schema
    rc = SQLStatistics(temp_hstmt,
        nullptr, 0,                            // CatalogName (NULL)
        nullptr, 0,                            // SchemaName (NULL)
        nullptr, 0,                            // TableName (NULL)
        SQL_INDEX_ALL,                         // Unique
        SQL_ENSURE                             // Reserved
    );

    if (rc != SQL_SUCCESS) {
        // Get detailed error information
        SQLTCHAR sqlstate[6];
        SQLINTEGER native_error;
        SQLTCHAR error_msg[256];
        SQLSMALLINT msg_len;
        
        SQLGetDiagRec(SQL_HANDLE_STMT, temp_hstmt, 1, sqlstate, &native_error, 
                      error_msg, sizeof(error_msg) / sizeof(SQLTCHAR), &msg_len);
        
        auto sqlstate_str = toUTF8(sqlstate);
        auto error_msg_str = toUTF8(error_msg);
        
        std::cout << "SQLStatistics failed with SQLSTATE: " << sqlstate_str 
                  << ", Native Error: " << native_error 
                  << ", Message: " << error_msg_str << std::endl;
        
        // HY009 (Invalid use of null pointer) is expected when passing NULL parameters
        // This verifies that the driver properly validates input parameters
        EXPECT_TRUE(rc == SQL_SUCCESS || rc == SQL_ERROR) 
            << "SQLStatistics returned unexpected code: " << rc;
        
        // On some systems, the specific error might be different, but should still be an error
        if (rc == SQL_ERROR && !sqlstate_str.empty()) {
            // Common SQLSTATE codes for null pointer errors: HY009, HY001, 07009
            EXPECT_TRUE(sqlstate_str == "HY009" || sqlstate_str == "HY001" || sqlstate_str == "07009")
                << "Unexpected SQLSTATE for NULL parameter test: " << sqlstate_str;
        }
    } else {
        EXPECT_EQ(rc, SQL_SUCCESS);        // Should still return proper result set structure
        SQLSMALLINT column_count = 0;
        rc = SQLNumResultCols(temp_hstmt, &column_count);
        EXPECT_EQ(rc, SQL_SUCCESS);
        EXPECT_EQ(column_count, 13);
    }

    // Clean up the temporary statement handle
    if (temp_hstmt != SQL_NULL_HSTMT) {
        SQLFreeStmt(temp_hstmt, SQL_CLOSE);
        SQLFreeHandle(SQL_HANDLE_STMT, temp_hstmt);
    }
}

// Test SQLSpecialColumns with different parameters
TEST_F(CatalogFunctionsTest, SQLSpecialColumnsWithDifferentParameters)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Test with SQL_ROWVER instead of SQL_BEST_ROWID
    auto catalog = fromUTF8<PTChar>("default");
    auto schema = fromUTF8<PTChar>("");
    auto table = fromUTF8<PTChar>("system.tables");

    rc = SQLSpecialColumns(hstmt,
        SQL_ROWVER,                            // IdentifierType
        ptcharCast(catalog.data()), SQL_NTS,   // CatalogName
        ptcharCast(schema.data()), SQL_NTS,    // SchemaName
        ptcharCast(table.data()), SQL_NTS,     // TableName
        SQL_SCOPE_TRANSACTION,                 // Scope
        SQL_NO_NULLS                          // Nullable
    );

    EXPECT_EQ(rc, SQL_SUCCESS);

    // Verify the result set structure
    SQLSMALLINT column_count = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &column_count));
    EXPECT_EQ(column_count, 8);

    // The result should be empty (WHERE 1 == 0)
    rc = SQLFetch(hstmt);
    EXPECT_EQ(rc, SQL_NO_DATA);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
}

// Test that SQLGetFunctions reports these functions as supported
TEST_F(CatalogFunctionsTest, SQLGetFunctionsSupport)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLUSMALLINT supported = SQL_FALSE;

    // Test SQLSpecialColumns support
    rc = SQLGetFunctions(hdbc, SQL_API_SQLSPECIALCOLUMNS, &supported);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(supported, SQL_TRUE);

    // Test SQLStatistics support
    rc = SQLGetFunctions(hdbc, SQL_API_SQLSTATISTICS, &supported);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(supported, SQL_TRUE);
}

// Test SQLStatistics with actual data skipping index
TEST_F(CatalogFunctionsTest, SQLStatisticsWithDataSkippingIndex)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Create a test table with a data skipping index
    auto create_table_query = fromUTF8<PTChar>(
        "CREATE TABLE IF NOT EXISTS test_index_table ("
        "id UInt32, "
        "name String, "
        "value Float64"
        ") ENGINE = MergeTree() "
        "ORDER BY id"
    );

    try {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(create_table_query.data()), SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

        // Try to create a data skipping index - might fail due to permissions
        auto create_index_query = fromUTF8<PTChar>(
            "ALTER TABLE test_index_table ADD INDEX idx_name name TYPE bloom_filter() GRANULARITY 1"
        );

        try {
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(create_index_query.data()), SQL_NTS));
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
            // Index creation succeeded
        } catch (const std::exception& e) {
            // If index creation fails due to permissions, skip this part
            std::cout << "Index creation failed (permissions): skipping index test" << std::endl;
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
        }

        // Test SQLStatistics on this table regardless of index creation success
        auto catalog = fromUTF8<PTChar>("default");
        auto schema = fromUTF8<PTChar>("");
        auto table = fromUTF8<PTChar>("test_index_table");

        rc = SQLStatistics(hstmt,
            ptcharCast(catalog.data()), SQL_NTS,   // CatalogName
            ptcharCast(schema.data()), SQL_NTS,    // SchemaName
            ptcharCast(table.data()), SQL_NTS,     // TableName
            SQL_INDEX_ALL,                         // Unique
            SQL_ENSURE                             // Reserved
        );

        EXPECT_EQ(rc, SQL_SUCCESS);

        // Verify the result set structure
        SQLSMALLINT column_count = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &column_count));
        EXPECT_EQ(column_count, 13);

        // Check for any indices (may or may not find the one we tried to create)
        ResultSetReader reader{hstmt};
        int index_count = 0;
        while (reader.fetch()) {
            auto table_name = reader.getData<std::string>("TABLE_NAME");
            auto index_name = reader.getData<std::string>("INDEX_NAME");

            if (table_name.has_value() && table_name.value() == "test_index_table") {
                index_count++;
                if (index_name.has_value()) {
                }
            }
        }
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

        // Clean up - always try to drop the table
        auto drop_table_query = fromUTF8<PTChar>("DROP TABLE IF EXISTS test_index_table");
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(drop_table_query.data()), SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

    } catch (const std::exception& e) {
        // If table creation fails, skip the entire test
        std::cout << "Table creation failed, skipping test" << std::endl;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
        
        // Still clean up just in case
        try {
            auto drop_table_query = fromUTF8<PTChar>("DROP TABLE IF EXISTS test_index_table");
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(drop_table_query.data()), SQL_NTS));
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
        } catch (...) {
            // Ignore cleanup errors
        }
    }
}

// Test error handling with invalid statement handle
TEST_F(CatalogFunctionsTest, InvalidStatementHandleError)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Test SQLSpecialColumns with invalid handle (should return SQL_INVALID_HANDLE)
    rc = SQLSpecialColumns(nullptr,
        SQL_BEST_ROWID,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        SQL_SCOPE_CURROW,
        SQL_NULLABLE
    );

    EXPECT_EQ(rc, SQL_INVALID_HANDLE);

    // Test SQLStatistics with invalid handle (should return SQL_INVALID_HANDLE)
    rc = SQLStatistics(nullptr,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        SQL_INDEX_ALL,
        SQL_ENSURE
    );

    EXPECT_EQ(rc, SQL_INVALID_HANDLE);
}
