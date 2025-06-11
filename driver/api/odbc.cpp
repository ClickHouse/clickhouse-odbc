#include "driver/platform/platform.h"
#include "driver/api/impl/impl.h"
#include "driver/utils/sql_encoding.h"
#include "driver/utils/utils.h"
#include "driver/api/sql_columns_resultset_mutator.h"
#include "driver/utils/type_parser.h"
#include "driver/attributes.h"
#include "driver/diagnostics.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"
#include "driver/result_set.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Timezone.h>

#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

#include <cstdio>
#include <cstring>

/** Functions from the ODBC interface can not directly call other functions.
  * Because not a function from this library will be called, but a wrapper from the driver manager,
  * which can work incorrectly, being called from within another function.
  * Wrong - because driver manager wraps all handle in its own,
  * which already have other addresses.
  * The actual implementation bodies are also moved out of from the ODBC interface calls,
  * to be out of extern "C" section, so that C++ features like generic lambdas, templates, are allowed,
  * for example, by MSVC.
  */

extern "C" {

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLAllocHandle)(SQLSMALLINT handle_type, SQLHANDLE input_handle, SQLHANDLE * output_handle) {
    LOG(__FUNCTION__ << " handle_type=" << handle_type << " input_handle=" << input_handle);

    switch (handle_type) {
        case SQL_HANDLE_ENV:
            return impl::allocEnv((SQLHENV *)output_handle);
        case SQL_HANDLE_DBC:
            return impl::allocConnect((SQLHENV)input_handle, (SQLHDBC *)output_handle);
        case SQL_HANDLE_STMT:
            return impl::allocStmt((SQLHDBC)input_handle, (SQLHSTMT *)output_handle);
        case SQL_HANDLE_DESC:
            return impl::allocDesc((SQLHDBC)input_handle, (SQLHDESC *)output_handle);
        default:
            LOG("AllocHandle: Unknown handleType=" << handle_type);
            return SQL_ERROR;
    }
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFreeHandle)(SQLSMALLINT handleType, SQLHANDLE handle) {
    LOG(__FUNCTION__ << " handleType=" << handleType << " handle=" << handle);

    switch (handleType) {
        case SQL_HANDLE_ENV:
        case SQL_HANDLE_DBC:
        case SQL_HANDLE_STMT:
        case SQL_HANDLE_DESC:
            return impl::freeHandle(handle);
        default:
            LOG("FreeHandle: Unknown handleType=" << handleType);
            return SQL_ERROR;
    }
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFreeStmt)(HSTMT statement_handle, SQLUSMALLINT option) {
    LOG(__FUNCTION__ << " option=" << option);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&] (Statement & statement) -> SQLRETURN {
        switch (option) {
            case SQL_CLOSE: /// Close the cursor, ignore the remaining results. If there is no cursor, then noop.
                statement.closeCursor();
                return SQL_SUCCESS;

            case SQL_DROP:
                return impl::freeHandle(statement_handle);

            case SQL_UNBIND:
                statement.resetColBindings();
                return SQL_SUCCESS;

            case SQL_RESET_PARAMS:
                statement.resetParamBindings();
                return SQL_SUCCESS;
        }

        return SQL_ERROR;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetInfo)(
    SQLHDBC         connection_handle,
    SQLUSMALLINT    info_type,
    SQLPOINTER      out_value,
    SQLSMALLINT     out_value_max_length,
    SQLSMALLINT *   out_value_length
) {
    LOG("GetInfo with info_type: " << info_type << ", out_value_max_length: " << out_value_max_length);

    /** How are all these values selected?
      * Part of them provides true information about the capabilities of the DBMS.
      * But in most cases, the possibilities are declared "in reserve" to see,
      * what requests will be sent and what any software will do, meaning these features.
      */

    auto func = [&](Connection & connection) -> SQLRETURN {
        const char * name = nullptr;

        constexpr auto mask_common_CONVERT_dest =
#ifdef SQL_CVT_GUID
            SQL_CVT_GUID |
#endif
            SQL_CVT_CHAR | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR |
            SQL_CVT_WCHAR | SQL_CVT_WVARCHAR | SQL_CVT_WLONGVARCHAR |
            SQL_CVT_BINARY | SQL_CVT_VARBINARY | SQL_CVT_LONGVARBINARY |
            SQL_CVT_BIT | SQL_CVT_TINYINT | SQL_CVT_SMALLINT | SQL_CVT_INTEGER | SQL_CVT_BIGINT |
            SQL_CVT_DECIMAL | SQL_CVT_NUMERIC | SQL_CVT_DOUBLE | SQL_CVT_FLOAT | SQL_CVT_REAL |
            SQL_CVT_DATE | SQL_CVT_TIME | SQL_CVT_TIMESTAMP | SQL_CVT_INTERVAL_YEAR_MONTH | SQL_CVT_INTERVAL_DAY_TIME
        ;

        switch (info_type) {

            case SQL_DBMS_VER:
                return impl::getServerVersion(connection_handle, out_value, out_value_max_length, out_value_length);

#define CASE_STRING(NAME, VALUE) \
            case NAME:           \
                return fillOutputString<PTChar>(VALUE, out_value, out_value_max_length, out_value_length, true);

            CASE_STRING(SQL_DRIVER_VER, VERSION_STRING)
            CASE_STRING(SQL_DRIVER_ODBC_VER, "03.80")
            CASE_STRING(SQL_DM_VER, "03.80.0000.0000")
            CASE_STRING(SQL_DRIVER_NAME, DRIVER_FILE_NAME)
            CASE_STRING(SQL_DBMS_NAME, "ClickHouse")
            CASE_STRING(SQL_SERVER_NAME, connection.server)
            CASE_STRING(SQL_DATA_SOURCE_NAME, connection.dsn)
            CASE_STRING(SQL_CATALOG_TERM, "catalog")
            CASE_STRING(SQL_COLLATION_SEQ, "UTF-8")
            CASE_STRING(SQL_DATABASE_NAME, connection.database)
            CASE_STRING(SQL_KEYWORDS, "")
            CASE_STRING(SQL_PROCEDURE_TERM, "stored procedure")
            CASE_STRING(SQL_CATALOG_NAME_SEPARATOR, ".")
            CASE_STRING(SQL_IDENTIFIER_QUOTE_CHAR, "`")
            CASE_STRING(SQL_SEARCH_PATTERN_ESCAPE, "\\")
            CASE_STRING(SQL_SCHEMA_TERM, "schema")
            CASE_STRING(SQL_TABLE_TERM, "table")
            CASE_STRING(SQL_SPECIAL_CHARACTERS, "")
            CASE_STRING(SQL_USER_NAME, connection.username)
            CASE_STRING(SQL_XOPEN_CLI_YEAR, "2015")

            CASE_FALLTHROUGH(SQL_DATA_SOURCE_READ_ONLY)
            CASE_FALLTHROUGH(SQL_ACCESSIBLE_PROCEDURES)
            CASE_FALLTHROUGH(SQL_ACCESSIBLE_TABLES)
            CASE_FALLTHROUGH(SQL_CATALOG_NAME)
            CASE_FALLTHROUGH(SQL_EXPRESSIONS_IN_ORDERBY)
            CASE_FALLTHROUGH(SQL_LIKE_ESCAPE_CLAUSE)
            CASE_FALLTHROUGH(SQL_MULTIPLE_ACTIVE_TXN)
            CASE_FALLTHROUGH(SQL_OUTER_JOINS)
            CASE_STRING(SQL_COLUMN_ALIAS, "Y")

            CASE_FALLTHROUGH(SQL_ORDER_BY_COLUMNS_IN_SELECT)
            CASE_FALLTHROUGH(SQL_INTEGRITY)
            CASE_FALLTHROUGH(SQL_MAX_ROW_SIZE_INCLUDES_LONG)
            CASE_FALLTHROUGH(SQL_MULT_RESULT_SETS)
            CASE_FALLTHROUGH(SQL_NEED_LONG_DATA_LEN)
            CASE_FALLTHROUGH(SQL_PROCEDURES)
            CASE_FALLTHROUGH(SQL_ROW_UPDATES)
            CASE_STRING(SQL_DESCRIBE_PARAMETER, "N")

            /// UINTEGER single values
            CASE_NUM(SQL_ODBC_INTERFACE_CONFORMANCE, SQLUINTEGER, SQL_OIC_CORE)
            CASE_NUM(SQL_ASYNC_MODE, SQLUINTEGER, SQL_AM_NONE)
#if defined(SQL_ASYNC_NOTIFICATION)
            CASE_NUM(SQL_ASYNC_NOTIFICATION, SQLUINTEGER, SQL_ASYNC_NOTIFICATION_NOT_CAPABLE)
#endif
            CASE_NUM(SQL_DEFAULT_TXN_ISOLATION, SQLUINTEGER, SQL_TXN_SERIALIZABLE)
#if defined(SQL_DRIVER_AWARE_POOLING_CAPABLE)
            CASE_NUM(SQL_DRIVER_AWARE_POOLING_SUPPORTED, SQLUINTEGER, SQL_DRIVER_AWARE_POOLING_CAPABLE)
#endif
            CASE_NUM(SQL_PARAM_ARRAY_ROW_COUNTS, SQLUINTEGER, SQL_PARC_BATCH)
            CASE_NUM(SQL_PARAM_ARRAY_SELECTS, SQLUINTEGER, SQL_PAS_BATCH)
            CASE_NUM(SQL_SQL_CONFORMANCE, SQLUINTEGER, SQL_SC_SQL92_ENTRY)

            /// USMALLINT single values
            CASE_NUM(SQL_ODBC_API_CONFORMANCE, SQLSMALLINT, SQL_OAC_LEVEL1);
            CASE_NUM(SQL_ODBC_SQL_CONFORMANCE, SQLSMALLINT, SQL_OSC_CORE);
            CASE_NUM(SQL_GROUP_BY, SQLUSMALLINT, SQL_GB_GROUP_BY_CONTAINS_SELECT)
            CASE_NUM(SQL_CATALOG_LOCATION, SQLUSMALLINT, SQL_CL_START)
            CASE_NUM(SQL_FILE_USAGE, SQLUSMALLINT, SQL_FILE_NOT_SUPPORTED)
            CASE_NUM(SQL_IDENTIFIER_CASE, SQLUSMALLINT, SQL_IC_SENSITIVE)
            CASE_NUM(SQL_QUOTED_IDENTIFIER_CASE, SQLUSMALLINT, SQL_IC_SENSITIVE)
            CASE_NUM(SQL_CONCAT_NULL_BEHAVIOR, SQLUSMALLINT, SQL_CB_NULL)
            CASE_NUM(SQL_CORRELATION_NAME, SQLUSMALLINT, SQL_CN_ANY)
            CASE_FALLTHROUGH(SQL_CURSOR_COMMIT_BEHAVIOR)
            CASE_NUM(SQL_CURSOR_ROLLBACK_BEHAVIOR, SQLUSMALLINT, SQL_CB_PRESERVE)
            CASE_NUM(SQL_CURSOR_SENSITIVITY, SQLUSMALLINT, SQL_INSENSITIVE)
            CASE_NUM(SQL_NON_NULLABLE_COLUMNS, SQLUSMALLINT, SQL_NNC_NON_NULL)
            CASE_NUM(SQL_NULL_COLLATION, SQLUSMALLINT, SQL_NC_END)
            CASE_NUM(SQL_TXN_CAPABLE, SQLUSMALLINT, SQL_TC_NONE)

            /// UINTEGER non-empty bitmasks
            CASE_NUM(SQL_CATALOG_USAGE, SQLUINTEGER, SQL_CU_DML_STATEMENTS | SQL_CU_TABLE_DEFINITION)
            CASE_NUM(SQL_AGGREGATE_FUNCTIONS,
                SQLUINTEGER,
                SQL_AF_ALL | SQL_AF_AVG | SQL_AF_COUNT | SQL_AF_DISTINCT | SQL_AF_MAX | SQL_AF_MIN | SQL_AF_SUM)
            CASE_NUM(SQL_ALTER_TABLE,
                SQLUINTEGER,
                SQL_AT_ADD_COLUMN_DEFAULT | SQL_AT_ADD_COLUMN_SINGLE | SQL_AT_DROP_COLUMN_DEFAULT | SQL_AT_SET_COLUMN_DEFAULT)
            CASE_NUM(SQL_CONVERT_FUNCTIONS, SQLUINTEGER, /*SQL_FN_CVT_CAST |*/ SQL_FN_CVT_CONVERT)
            CASE_NUM(SQL_CREATE_TABLE, SQLUINTEGER, SQL_CT_CREATE_TABLE)
            CASE_NUM(SQL_CREATE_VIEW, SQLUINTEGER, SQL_CV_CREATE_VIEW)
            CASE_NUM(SQL_DROP_TABLE, SQLUINTEGER, SQL_DT_DROP_TABLE)
            CASE_NUM(SQL_DROP_VIEW, SQLUINTEGER, SQL_DV_DROP_VIEW)
            CASE_NUM(SQL_DATETIME_LITERALS, SQLUINTEGER, SQL_DL_SQL92_DATE | SQL_DL_SQL92_TIMESTAMP)
            CASE_NUM(SQL_GETDATA_EXTENSIONS, SQLUINTEGER, SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BOUND)
            CASE_NUM(SQL_INDEX_KEYWORDS, SQLUINTEGER, SQL_IK_NONE)
            CASE_NUM(SQL_INSERT_STATEMENT, SQLUINTEGER, SQL_IS_INSERT_LITERALS | SQL_IS_INSERT_SEARCHED)
            CASE_NUM(SQL_SCROLL_OPTIONS, SQLUINTEGER, SQL_SO_FORWARD_ONLY)
            CASE_NUM(SQL_SQL92_DATETIME_FUNCTIONS, SQLUINTEGER, SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME | SQL_SDF_CURRENT_TIMESTAMP)

#if defined(SQL_CONVERT_GUID)
            CASE_FALLTHROUGH(SQL_CONVERT_GUID)
#endif
            CASE_FALLTHROUGH(SQL_CONVERT_CHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_VARCHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_LONGVARCHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_WCHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_WVARCHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_WLONGVARCHAR)
            CASE_FALLTHROUGH(SQL_CONVERT_BINARY)
            CASE_FALLTHROUGH(SQL_CONVERT_VARBINARY)
            CASE_FALLTHROUGH(SQL_CONVERT_LONGVARBINARY)
            CASE_FALLTHROUGH(SQL_CONVERT_BIT)
            CASE_FALLTHROUGH(SQL_CONVERT_TINYINT)
            CASE_FALLTHROUGH(SQL_CONVERT_SMALLINT)
            CASE_FALLTHROUGH(SQL_CONVERT_INTEGER)
            CASE_FALLTHROUGH(SQL_CONVERT_BIGINT)
            CASE_FALLTHROUGH(SQL_CONVERT_DECIMAL)
            CASE_FALLTHROUGH(SQL_CONVERT_NUMERIC)
            CASE_FALLTHROUGH(SQL_CONVERT_DOUBLE)
            CASE_FALLTHROUGH(SQL_CONVERT_FLOAT)
            CASE_FALLTHROUGH(SQL_CONVERT_REAL)
            CASE_FALLTHROUGH(SQL_CONVERT_DATE)
            CASE_FALLTHROUGH(SQL_CONVERT_TIME)
            CASE_FALLTHROUGH(SQL_CONVERT_TIMESTAMP)
            CASE_FALLTHROUGH(SQL_CONVERT_INTERVAL_YEAR_MONTH)
            CASE_NUM(SQL_CONVERT_INTERVAL_DAY_TIME, SQLUINTEGER, mask_common_CONVERT_dest)

            CASE_NUM(SQL_NUMERIC_FUNCTIONS,
                SQLUINTEGER,
                SQL_FN_NUM_ABS | SQL_FN_NUM_ACOS | SQL_FN_NUM_ASIN | SQL_FN_NUM_ATAN | SQL_FN_NUM_ATAN2 | SQL_FN_NUM_CEILING
                    | SQL_FN_NUM_COS | SQL_FN_NUM_COT | SQL_FN_NUM_DEGREES | SQL_FN_NUM_EXP | SQL_FN_NUM_FLOOR | SQL_FN_NUM_LOG
                    | SQL_FN_NUM_LOG10 | SQL_FN_NUM_MOD | SQL_FN_NUM_PI | SQL_FN_NUM_POWER | SQL_FN_NUM_RADIANS | SQL_FN_NUM_RAND
                    | SQL_FN_NUM_ROUND | SQL_FN_NUM_SIGN | SQL_FN_NUM_SIN | SQL_FN_NUM_SQRT | SQL_FN_NUM_TAN | SQL_FN_NUM_TRUNCATE)

            CASE_NUM(SQL_OJ_CAPABILITIES,
                SQLUINTEGER,
                SQL_OJ_LEFT | SQL_OJ_RIGHT | SQL_OJ_INNER | SQL_OJ_FULL | SQL_OJ_NESTED | SQL_OJ_NOT_ORDERED | SQL_OJ_ALL_COMPARISON_OPS)

            CASE_NUM(SQL_SQL92_NUMERIC_VALUE_FUNCTIONS,
                SQLUINTEGER,
                SQL_SNVF_BIT_LENGTH | SQL_SNVF_CHAR_LENGTH | SQL_SNVF_CHARACTER_LENGTH | SQL_SNVF_EXTRACT | SQL_SNVF_OCTET_LENGTH
                    | SQL_SNVF_POSITION)

            CASE_NUM(SQL_SQL92_PREDICATES,
                SQLUINTEGER,
                SQL_SP_BETWEEN | SQL_SP_COMPARISON | SQL_SP_EXISTS | SQL_SP_IN | SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_LIKE
                    | SQL_SP_MATCH_FULL | SQL_SP_MATCH_PARTIAL | SQL_SP_MATCH_UNIQUE_FULL | SQL_SP_MATCH_UNIQUE_PARTIAL | SQL_SP_OVERLAPS
                    | SQL_SP_QUANTIFIED_COMPARISON | SQL_SP_UNIQUE)

            CASE_NUM(SQL_SQL92_RELATIONAL_JOIN_OPERATORS,
                SQLUINTEGER,
                /*SQL_SRJO_CORRESPONDING_CLAUSE |*/ SQL_SRJO_CROSS_JOIN | /*SQL_SRJO_EXCEPT_JOIN |*/ SQL_SRJO_FULL_OUTER_JOIN
                    | SQL_SRJO_INNER_JOIN | /*SQL_SRJO_INTERSECT_JOIN |*/
                    SQL_SRJO_LEFT_OUTER_JOIN | /*SQL_SRJO_NATURAL_JOIN |*/ SQL_SRJO_RIGHT_OUTER_JOIN /*| SQL_SRJO_UNION_JOIN*/)

            CASE_NUM(SQL_SQL92_ROW_VALUE_CONSTRUCTOR,
                SQLUINTEGER,
                SQL_SRVC_VALUE_EXPRESSION | SQL_SRVC_NULL | SQL_SRVC_DEFAULT | SQL_SRVC_ROW_SUBQUERY)

            CASE_NUM(SQL_SQL92_STRING_FUNCTIONS,
                SQLUINTEGER,
                SQL_SSF_CONVERT | SQL_SSF_LOWER | SQL_SSF_UPPER | SQL_SSF_SUBSTRING | SQL_SSF_TRANSLATE | SQL_SSF_TRIM_BOTH
                    | SQL_SSF_TRIM_LEADING | SQL_SSF_TRIM_TRAILING)

            CASE_NUM(SQL_SQL92_VALUE_EXPRESSIONS, SQLUINTEGER, SQL_SVE_CASE | SQL_SVE_CAST | SQL_SVE_COALESCE | SQL_SVE_NULLIF)

            CASE_NUM(SQL_STANDARD_CLI_CONFORMANCE, SQLUINTEGER, SQL_SCC_XOPEN_CLI_VERSION1 | SQL_SCC_ISO92_CLI)

            CASE_NUM(SQL_STRING_FUNCTIONS,
                SQLUINTEGER,
                SQL_FN_STR_ASCII | SQL_FN_STR_BIT_LENGTH | SQL_FN_STR_CHAR | SQL_FN_STR_CHAR_LENGTH | SQL_FN_STR_CHARACTER_LENGTH
                    | SQL_FN_STR_CONCAT | SQL_FN_STR_DIFFERENCE | SQL_FN_STR_INSERT | SQL_FN_STR_LCASE | SQL_FN_STR_LEFT | SQL_FN_STR_LENGTH
                    | SQL_FN_STR_LOCATE | SQL_FN_STR_LTRIM | SQL_FN_STR_OCTET_LENGTH | SQL_FN_STR_POSITION | SQL_FN_STR_REPEAT
                    | SQL_FN_STR_REPLACE | SQL_FN_STR_RIGHT | SQL_FN_STR_RTRIM | SQL_FN_STR_SOUNDEX | SQL_FN_STR_SPACE
                    | SQL_FN_STR_SUBSTRING | SQL_FN_STR_UCASE)

            CASE_NUM(SQL_SUBQUERIES,
                SQLUINTEGER,
                /*SQL_SQ_CORRELATED_SUBQUERIES |*/ SQL_SQ_COMPARISON | SQL_SQ_EXISTS | SQL_SQ_IN | SQL_SQ_QUANTIFIED)

            CASE_NUM(SQL_TIMEDATE_ADD_INTERVALS,
                SQLUINTEGER,
                SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_SECOND | SQL_FN_TSI_MINUTE | SQL_FN_TSI_HOUR | SQL_FN_TSI_DAY | SQL_FN_TSI_WEEK
                    | SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | SQL_FN_TSI_YEAR)

            CASE_NUM(SQL_TIMEDATE_DIFF_INTERVALS,
                SQLUINTEGER,
                SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_SECOND | SQL_FN_TSI_MINUTE | SQL_FN_TSI_HOUR | SQL_FN_TSI_DAY | SQL_FN_TSI_WEEK
                    | SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | SQL_FN_TSI_YEAR)

            CASE_NUM(SQL_TIMEDATE_FUNCTIONS,
                SQLUINTEGER,
                SQL_FN_TD_CURRENT_DATE | SQL_FN_TD_CURRENT_TIME | SQL_FN_TD_CURRENT_TIMESTAMP | SQL_FN_TD_CURDATE | SQL_FN_TD_CURTIME
                    | SQL_FN_TD_DAYNAME | SQL_FN_TD_DAYOFMONTH | SQL_FN_TD_DAYOFWEEK | SQL_FN_TD_DAYOFYEAR | SQL_FN_TD_EXTRACT
                    | SQL_FN_TD_HOUR | SQL_FN_TD_MINUTE | SQL_FN_TD_MONTH | SQL_FN_TD_MONTHNAME | SQL_FN_TD_NOW | SQL_FN_TD_QUARTER
                    | SQL_FN_TD_SECOND | SQL_FN_TD_TIMESTAMPADD | SQL_FN_TD_TIMESTAMPDIFF | SQL_FN_TD_WEEK | SQL_FN_TD_YEAR)

            CASE_NUM(SQL_TXN_ISOLATION_OPTION, SQLUINTEGER, SQL_TXN_SERIALIZABLE)

            CASE_NUM(SQL_UNION, SQLUINTEGER, SQL_U_UNION | SQL_U_UNION_ALL)

            /// UINTEGER empty bitmasks
            CASE_FALLTHROUGH(SQL_ALTER_DOMAIN)
            CASE_FALLTHROUGH(SQL_BATCH_ROW_COUNT)
            CASE_FALLTHROUGH(SQL_BATCH_SUPPORT)
            CASE_FALLTHROUGH(SQL_BOOKMARK_PERSISTENCE)
            CASE_FALLTHROUGH(SQL_CREATE_ASSERTION)
            CASE_FALLTHROUGH(SQL_CREATE_CHARACTER_SET)
            CASE_FALLTHROUGH(SQL_CREATE_COLLATION)
            CASE_FALLTHROUGH(SQL_CREATE_DOMAIN)
            CASE_FALLTHROUGH(SQL_CREATE_SCHEMA)
            CASE_FALLTHROUGH(SQL_CREATE_TRANSLATION)
            CASE_FALLTHROUGH(SQL_DROP_ASSERTION)
            CASE_FALLTHROUGH(SQL_DROP_CHARACTER_SET)
            CASE_FALLTHROUGH(SQL_DROP_COLLATION)
            CASE_FALLTHROUGH(SQL_DROP_DOMAIN)
            CASE_FALLTHROUGH(SQL_DROP_SCHEMA)
            CASE_FALLTHROUGH(SQL_DROP_TRANSLATION)
            CASE_FALLTHROUGH(SQL_DYNAMIC_CURSOR_ATTRIBUTES1)
            CASE_FALLTHROUGH(SQL_DYNAMIC_CURSOR_ATTRIBUTES2)
            CASE_FALLTHROUGH(SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1)
            CASE_FALLTHROUGH(SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2)
            CASE_FALLTHROUGH(SQL_KEYSET_CURSOR_ATTRIBUTES1)
            CASE_FALLTHROUGH(SQL_KEYSET_CURSOR_ATTRIBUTES2)
            CASE_FALLTHROUGH(SQL_STATIC_CURSOR_ATTRIBUTES1)
            CASE_FALLTHROUGH(SQL_STATIC_CURSOR_ATTRIBUTES2)
            CASE_FALLTHROUGH(SQL_INFO_SCHEMA_VIEWS)
            CASE_FALLTHROUGH(SQL_POS_OPERATIONS)
            CASE_FALLTHROUGH(SQL_SCHEMA_USAGE)
            CASE_FALLTHROUGH(SQL_SYSTEM_FUNCTIONS)
            CASE_FALLTHROUGH(SQL_SQL92_FOREIGN_KEY_DELETE_RULE)
            CASE_FALLTHROUGH(SQL_SQL92_FOREIGN_KEY_UPDATE_RULE)
            CASE_FALLTHROUGH(SQL_SQL92_GRANT)
            CASE_FALLTHROUGH(SQL_SQL92_REVOKE)
            CASE_FALLTHROUGH(SQL_STATIC_SENSITIVITY)
            CASE_FALLTHROUGH(SQL_LOCK_TYPES)
            CASE_FALLTHROUGH(SQL_SCROLL_CONCURRENCY)
            CASE_NUM(SQL_DDL_INDEX, SQLUINTEGER, 0)

            /// Limits on the maximum number, USMALLINT.
            CASE_FALLTHROUGH(SQL_ACTIVE_ENVIRONMENTS)
            CASE_FALLTHROUGH(SQL_MAX_COLUMNS_IN_GROUP_BY)
            CASE_FALLTHROUGH(SQL_MAX_COLUMNS_IN_INDEX)
            CASE_FALLTHROUGH(SQL_MAX_COLUMNS_IN_ORDER_BY)
            CASE_FALLTHROUGH(SQL_MAX_COLUMNS_IN_SELECT)
            CASE_FALLTHROUGH(SQL_MAX_COLUMNS_IN_TABLE)
            CASE_FALLTHROUGH(SQL_MAX_CONCURRENT_ACTIVITIES)
            CASE_FALLTHROUGH(SQL_MAX_DRIVER_CONNECTIONS)
            CASE_FALLTHROUGH(SQL_MAX_IDENTIFIER_LEN)
            CASE_FALLTHROUGH(SQL_MAX_PROCEDURE_NAME_LEN)
            CASE_FALLTHROUGH(SQL_MAX_TABLES_IN_SELECT)
            CASE_FALLTHROUGH(SQL_MAX_USER_NAME_LEN)
            CASE_FALLTHROUGH(SQL_MAX_COLUMN_NAME_LEN)
            CASE_FALLTHROUGH(SQL_MAX_CURSOR_NAME_LEN)
            CASE_FALLTHROUGH(SQL_MAX_SCHEMA_NAME_LEN)
            CASE_FALLTHROUGH(SQL_MAX_TABLE_NAME_LEN)
            CASE_NUM(SQL_MAX_CATALOG_NAME_LEN, SQLUSMALLINT, 0)

            /// Limitations on the maximum number, UINTEGER.
            CASE_FALLTHROUGH(SQL_MAX_ROW_SIZE)
            CASE_FALLTHROUGH(SQL_MAX_STATEMENT_LEN)
            CASE_FALLTHROUGH(SQL_MAX_BINARY_LITERAL_LEN)
            CASE_FALLTHROUGH(SQL_MAX_CHAR_LITERAL_LEN)
            CASE_FALLTHROUGH(SQL_MAX_INDEX_SIZE)
            CASE_NUM(SQL_MAX_ASYNC_CONCURRENT_STATEMENTS, SQLUINTEGER, 0)

#if defined(SQL_ASYNC_DBC_FUNCTIONS)
            CASE_NUM(SQL_ASYNC_DBC_FUNCTIONS, SQLUINTEGER, 0)
#endif

            default:
                throw std::runtime_error("Unsupported info type: " + std::to_string(info_type));

#undef CASE_STRING

        }
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLSetEnvAttr)(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetEnvAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetConnectAttr)(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetConnectAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetStmtAttr)(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetStmtAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLGetEnvAttr)(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetEnvAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetConnectAttr)(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetConnectAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetStmtAttr)(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetStmtAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLConnect)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     UserName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     Authentication,
    SQLSMALLINT    NameLength3
) {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, ConnectionHandle, [&](Connection & connection) {
        std::string connection_string;
        if (ServerName) {
            connection_string += "DSN={";
            connection_string += toUTF8(ServerName, NameLength1);
            connection_string += "};";
        }
        if (UserName) {
            connection_string += "UID={";
            connection_string += toUTF8(UserName, NameLength2);
            connection_string += "};";
        }
        if (Authentication) {
            connection_string += "PWD={";
            connection_string += toUTF8(Authentication, NameLength3);
            connection_string += "};";
        }
        connection.connect(connection_string);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLDriverConnect)(
    SQLHDBC         ConnectionHandle,
    SQLHWND         WindowHandle,
    SQLTCHAR *      InConnectionString,
    SQLSMALLINT     StringLength1,
    SQLTCHAR *      OutConnectionString,
    SQLSMALLINT     BufferLength,
    SQLSMALLINT *   StringLength2Ptr,
    SQLUSMALLINT    DriverCompletion
) {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, ConnectionHandle, [&](Connection & connection) {
        const auto connection_string = toUTF8(InConnectionString, StringLength1);

        auto out_buffer_length = BufferLength;
        if (out_buffer_length <= 0) {
            if (StringLength1 > 0)
                out_buffer_length = StringLength1;
            else if (StringLength1 == SQL_NTS)
                out_buffer_length = stringBufferLength(InConnectionString);
            else
                out_buffer_length = 1024; // ...as per SQLDriverConnect() doc: "Applications should allocate at least 1,024 characters for this buffer."
        }

        connection.connect(connection_string);
        return fillOutputString<PTChar>(connection_string, OutConnectionString, out_buffer_length, StringLength2Ptr, false);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLPrepare)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    //LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        const auto query = toUTF8(statement_text, statement_text_size);
        statement.prepareQuery(query);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLExecute)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        statement.executeQuery();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLExecDirect)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    //LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        const auto query = toUTF8(statement_text, statement_text_size);
        statement.executeQuery(query);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLNumResultCols)(
    SQLHSTMT        StatementHandle,
    SQLSMALLINT *   ColumnCountPtr
) {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, [&](Statement & statement) {
        if (ColumnCountPtr) {
            if (statement.isPrepared() && !statement.isExecuted())
                statement.forwardExecuteQuery();

            if (statement.hasResultSet()) {
                auto & result_set = statement.getResultSet();
                *ColumnCountPtr = result_set.getColumnCount();
            }
            else {
                *ColumnCountPtr = 0;
            }
        }

        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColAttribute)(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLUSMALLINT field_identifier,
    SQLPOINTER out_string_value,
    SQLSMALLINT out_string_value_max_size,
    SQLSMALLINT * out_string_value_size,
#if defined(_win32_) && !defined(_win64_)
    SQLPOINTER out_num_value
#else
    SQLLEN * out_num_value
#endif
) {
    LOG(__FUNCTION__ << "(col=" << column_number << ", field=" << field_identifier << ")");
    auto func = [&](Statement & statement) -> SQLRETURN {
        if (!statement.hasResultSet())
            throw SqlException("Column info is not available", "07005");

        auto & result_set = statement.getResultSet();

        if (column_number < 1)
            throw SqlException("Invalid descriptor index", "07009");

        const auto column_idx = column_number - 1;
        const auto & column_info = result_set.getColumnInfo(column_idx);
        const auto & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);

        std::int32_t SQL_DESC_LENGTH_value = 0;
        if (type_info.isBufferType()) {
            if (column_info.display_size > 0)
                SQL_DESC_LENGTH_value = column_info.display_size;
            else if (column_info.fixed_size > 0)
                SQL_DESC_LENGTH_value = column_info.fixed_size;

            if (SQL_DESC_LENGTH_value <= 0 || SQL_DESC_LENGTH_value > statement.getParent().stringmaxlength)
                SQL_DESC_LENGTH_value = statement.getParent().stringmaxlength;
        }

        std::int32_t SQL_DESC_OCTET_LENGTH_value = type_info.octet_length;
        if (type_info.isBufferType()) {
            if (type_info.isWideCharStringType())
                SQL_DESC_OCTET_LENGTH_value = SQL_DESC_LENGTH_value * sizeof(SQLWCHAR);
            else
                SQL_DESC_OCTET_LENGTH_value = SQL_DESC_LENGTH_value * sizeof(SQLCHAR);
        }

        switch (field_identifier) {

#define CASE_FIELD_NUM(NAME, VALUE)                                     \
            case NAME:                                                  \
                if (out_num_value)                                      \
                    *reinterpret_cast<SQLLEN *>(out_num_value) = VALUE; \
                return SQL_SUCCESS;

#define CASE_FIELD_STR(NAME, VALUE) \
            case NAME: return fillOutputString<PTChar>(VALUE, out_string_value, out_string_value_max_size, out_string_value_size, true);

            // TODO: Use IRD (the descriptor) when column data representation is migrated there.

            CASE_FIELD_NUM(SQL_DESC_AUTO_UNIQUE_VALUE, SQL_FALSE);
            CASE_FIELD_STR(SQL_DESC_BASE_COLUMN_NAME, column_info.name);
            CASE_FIELD_STR(SQL_DESC_BASE_TABLE_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_CASE_SENSITIVE, SQL_TRUE);
            CASE_FIELD_STR(SQL_DESC_CATALOG_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_CONCISE_TYPE, type_info.data_type);

            case SQL_COLUMN_COUNT: /* fallthrough */
            CASE_FIELD_NUM(SQL_DESC_COUNT, result_set.getColumnCount());

            CASE_FIELD_NUM(SQL_DESC_DISPLAY_SIZE, column_info.display_size);
            CASE_FIELD_NUM(SQL_DESC_FIXED_PREC_SCALE, SQL_FALSE);
            CASE_FIELD_STR(SQL_DESC_LABEL, column_info.name);

            case SQL_COLUMN_LENGTH: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_LENGTH, SQL_DESC_LENGTH_value);

            CASE_FIELD_STR(SQL_DESC_LITERAL_PREFIX, "");
            CASE_FIELD_STR(SQL_DESC_LITERAL_SUFFIX, "");
            CASE_FIELD_STR(SQL_DESC_LOCAL_TYPE_NAME, "");

            case SQL_COLUMN_NAME: /* fallthrough */
            CASE_FIELD_STR(SQL_DESC_NAME, column_info.name);

            case SQL_COLUMN_NULLABLE: /* fallthrough */
            CASE_FIELD_NUM(SQL_DESC_NULLABLE, (column_info.is_nullable ? SQL_NULLABLE : SQL_NO_NULLS));

            CASE_FIELD_NUM(SQL_DESC_NUM_PREC_RADIX, (type_info.isIntegerType() ? 10 : 0));
            CASE_FIELD_NUM(SQL_DESC_OCTET_LENGTH, SQL_DESC_OCTET_LENGTH_value);

            case SQL_COLUMN_PRECISION: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_PRECISION, 0);

            case SQL_COLUMN_SCALE: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_SCALE, 0);

            CASE_FIELD_STR(SQL_DESC_SCHEMA_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_SEARCHABLE, SQL_SEARCHABLE);
            CASE_FIELD_STR(SQL_DESC_TABLE_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_TYPE, type_info.data_type);
            CASE_FIELD_STR(SQL_DESC_TYPE_NAME, type_info.type_name);
            CASE_FIELD_NUM(SQL_DESC_UNNAMED, SQL_NAMED);
            CASE_FIELD_NUM(SQL_DESC_UNSIGNED,
                (type_info.unsigned_attribute == UnsignedAttribute::Unsigned ? SQL_TRUE : SQL_FALSE));
#ifdef SQL_ATTR_READ_ONLY
            CASE_FIELD_NUM(SQL_DESC_UPDATABLE, SQL_ATTR_READ_ONLY);
#else
            CASE_FIELD_NUM(SQL_DESC_UPDATABLE, SQL_FALSE);
#endif

#undef CASE_FIELD_NUM
#undef CASE_FIELD_STR

            default:
                throw SqlException("Driver not capable", "HYC00");
        }
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLDescribeCol)(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLTCHAR * out_column_name,
    SQLSMALLINT out_column_name_max_size,
    SQLSMALLINT * out_column_name_size,
    SQLSMALLINT * out_type,
    SQLULEN * out_column_size,
    SQLSMALLINT * out_decimal_digits,
    SQLSMALLINT * out_is_nullable
) {
    auto func = [&] (Statement & statement) {
        if (!statement.hasResultSet())
            throw SqlException("Column info is not available", "07005");

        auto & result_set = statement.getResultSet();

        if (column_number < 1)
            throw SqlException("Invalid descriptor index", "07009");

        const auto column_idx = column_number - 1;
        const auto & column_info = result_set.getColumnInfo(column_idx);
        const auto & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);

        LOG(__FUNCTION__ << " column_number=" << column_number << " name=" << column_info.name << " type=" << type_info.data_type
                         << " size=" << type_info.column_size << " nullable=" << column_info.is_nullable);

        if (out_type)
            *out_type = type_info.data_type;
        if (out_column_size)
            *out_column_size = std::min<int32_t>(
                statement.getParent().stringmaxlength, column_info.fixed_size ? column_info.fixed_size : type_info.column_size);
        if (out_decimal_digits)
            *out_decimal_digits = 0;
        if (out_is_nullable)
            *out_is_nullable = column_info.is_nullable ? SQL_NULLABLE : SQL_NO_NULLS;

        return fillOutputString<PTChar>(column_info.name, out_column_name, out_column_name_max_size, out_column_name_size, false);
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFetch)(
    SQLHSTMT     StatementHandle
) {
    LOG(__FUNCTION__);
    return impl::Fetch(
        StatementHandle
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFetchScroll)(
    SQLHSTMT      StatementHandle,
    SQLSMALLINT   FetchOrientation,
    SQLLEN        FetchOffset
) {
    LOG(__FUNCTION__);
    return impl::FetchScroll(
        StatementHandle,
        FetchOrientation,
        FetchOffset
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLGetData)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Col_or_Param_Num,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr
) {
    LOG(__FUNCTION__);
    return impl::GetData(
        StatementHandle,
        Col_or_Param_Num,
        TargetType,
        TargetValuePtr,
        BufferLength,
        StrLen_or_IndPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBindCol)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_Ind
) {
    LOG(__FUNCTION__);
    return impl::BindCol(
        StatementHandle,
        ColumnNumber,
        TargetType,
        TargetValuePtr,
        BufferLength,
        StrLen_or_Ind
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLRowCount)(HSTMT statement_handle, SQLLEN * out_row_count) {
    LOG(__FUNCTION__);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        if (out_row_count) {
            *out_row_count = statement.getDiagHeader().getAttrAs<SQLLEN>(SQL_DIAG_ROW_COUNT, 0);
            LOG("getNumRows=" << *out_row_count);
        }
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLMoreResults)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        return (statement.advanceToNextResultSet() ? SQL_SUCCESS : SQL_NO_DATA);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDisconnect)(HDBC connection_handle) {
    LOG(__FUNCTION__);
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, [&](Connection & connection) {
        connection.session->reset();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDiagRec)(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagRec(
        handle_type,
        handle,
        record_number,
        out_sqlstate,
        out_native_error_code,
        out_message,
        out_message_max_size,
        out_message_size
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDiagField)(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagField(
        handle_type,
        handle,
        record_number,
        field_id,
        out_message,
        out_message_max_size,
        out_message_size
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLTables)(
    SQLHSTMT        StatementHandle,
    SQLTCHAR *      CatalogName,
    SQLSMALLINT     NameLength1,
    SQLTCHAR *      SchemaName,
    SQLSMALLINT     NameLength2,
    SQLTCHAR *      TableName,
    SQLSMALLINT     NameLength3,
    SQLTCHAR *      TableType,
    SQLSMALLINT     NameLength4
) {
    auto func = [&](Statement & statement) {
        constexpr bool null_catalog_defaults_to_connected_database = true; // TODO: review and remove this behavior?
        const auto catalog = (CatalogName ? toUTF8(CatalogName, NameLength1) :
            (null_catalog_defaults_to_connected_database ? statement.getParent().database : SQL_ALL_CATALOGS));
        const auto schema = (SchemaName ? toUTF8(SchemaName, NameLength2) : SQL_ALL_SCHEMAS);
        const auto table = (TableName ? toUTF8(TableName, NameLength3) : "%");
        const auto table_type_list = (TableType ? toUTF8(TableType, NameLength4) : SQL_ALL_TABLE_TYPES);

        // N.B.: here, an empty 'catalog', 'schema', 'table', or 'table_type_list' variable would mean, that an empty string
        // has been supplied, not a nullptr. In case of nullptr, it would contain "%".

        std::stringstream query;
        query << "SELECT";

        // Get a list of all databases.
        if (catalog == SQL_ALL_CATALOGS && schema.empty() && table.empty()) {
            query << " CAST(name, 'Nullable(String)') AS TABLE_CAT,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_SCHEM,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_NAME,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_TYPE,";
            query << " CAST(NULL, 'Nullable(String)') AS REMARKS";
            query << " FROM system.databases";
        }
        // Get a list of all schemas (currently, just an empty list).
        else if (catalog.empty() && schema == SQL_ALL_SCHEMAS && table.empty()) {
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_CAT,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_SCHEM,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_NAME,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_TYPE,";
            query << " CAST(NULL, 'Nullable(String)') AS REMARKS";
            query << " WHERE (1 == 0)";
        }
        // Get a list of all valid table types (currently, 'TABLE' only.)
        else if (catalog.empty() && schema.empty() && table.empty() && table_type_list == SQL_ALL_TABLE_TYPES) {
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_CAT,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_SCHEM,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_NAME,";
            query << " CAST('TABLE', 'Nullable(String)') AS TABLE_TYPE,";
            query << " CAST(NULL, 'Nullable(String)') AS REMARKS";
        }
        // Get a list of tables matching all criteria.
        else {
            query << " CAST(database, 'Nullable(String)') AS TABLE_CAT,";
            query << " CAST(NULL, 'Nullable(String)') AS TABLE_SCHEM,";
            query << " CAST(name, 'Nullable(String)') AS TABLE_NAME,";
            query << " CAST('TABLE', 'Nullable(String)') AS TABLE_TYPE,";
            query << " CAST(NULL, 'Nullable(String)') AS REMARKS";
            query << " FROM system.tables";
            query << " WHERE (1 == 1)";

            // Completely ommit the condition part of the query, if the value of SQL_ATTR_METADATA_ID is SQL_TRUE
            // (i.e., values for the components are not patterns), and the component hasn't been supplied at all
            // (i.e. is nullptr; note, that actual empty strings are considered "supplied".)

            const auto is_odbc_v2 = (statement.getParent().getParent().odbc_version == SQL_OV_ODBC2);
            const auto is_pattern = (statement.getParent().getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE) != SQL_TRUE);
            const auto table_types = parseCatalogFnVLArgs(table_type_list);

            // TODO: Use of coalesce() is a workaround here. Review.

            // Note, that 'catalog' variable will be set to "%" above (or to the connected database name), even if CatalogName == nullptr.
            if (is_pattern && !is_odbc_v2) {
                if (!isMatchAnythingCatalogFnPatternArg(catalog))
                    query << " AND isNotNull(TABLE_CAT) AND coalesce(TABLE_CAT, '') LIKE '" << escapeForSQL(catalog) << "'";
            }
            else if (CatalogName) {
                query << " AND isNotNull(TABLE_CAT) AND coalesce(TABLE_CAT, '') == '" << escapeForSQL(catalog) << "'";
            }

            // Note, that 'schema' variable will be set to "%" above, even if SchemaName == nullptr.
            if (is_pattern) {
                if (!isMatchAnythingCatalogFnPatternArg(schema))
                    query << " AND isNotNull(TABLE_SCHEM) AND coalesce(TABLE_SCHEM, '') LIKE '" << escapeForSQL(schema) << "'";
            }
            else if (SchemaName) {
                query << " AND isNotNull(TABLE_SCHEM) AND coalesce(TABLE_SCHEM, '') == '" << escapeForSQL(schema) << "'";
            }

            // Note, that 'table' variable will be set to "%" above, even if TableName == nullptr.
            if (is_pattern) {
                if (!isMatchAnythingCatalogFnPatternArg(table))
                    query << " AND isNotNull(TABLE_NAME) AND coalesce(TABLE_NAME, '') LIKE '" << escapeForSQL(table) << "'";
            }
            else if (TableName) {
                query << " AND isNotNull(TABLE_NAME) AND coalesce(TABLE_NAME, '') == '" << escapeForSQL(table) << "'";
            }

            // Table type list is not affected by the value of SQL_ATTR_METADATA_ID, so we always treat it as a list of patterns.
            if (!table_types.empty()) {
                bool has_match_anything = false;
                for (const auto & table_type : table_types) {
                    has_match_anything = has_match_anything || isMatchAnythingCatalogFnPatternArg(table_type);
                }
                if (!has_match_anything) {
                    query << " AND isNotNull(TABLE_TYPE) AND (1 == 0";
                    for (const auto & table_type : table_types) {
                        query << " OR coalesce(TABLE_TYPE, '') LIKE '" << escapeForSQL(table_type) << "'";
                    }
                    query << ")";
                }
            }
        }

        query << " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        statement.executeQuery(query.str());

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColumns)(
    SQLHSTMT        StatementHandle,
    SQLTCHAR *      CatalogName,
    SQLSMALLINT     NameLength1,
    SQLTCHAR *      SchemaName,
    SQLSMALLINT     NameLength2,
    SQLTCHAR *      TableName,
    SQLSMALLINT     NameLength3,
    SQLTCHAR *      ColumnName,
    SQLSMALLINT     NameLength4
) {

    auto func = [&](Statement & statement) {
        constexpr bool null_catalog_defaults_to_connected_database = true; // TODO: review and remove this behavior?
        const auto catalog = (CatalogName ? toUTF8(CatalogName, NameLength1) :
            (null_catalog_defaults_to_connected_database ? statement.getParent().database : SQL_ALL_CATALOGS));
        const auto schema = (SchemaName ? toUTF8(SchemaName, NameLength2) : SQL_ALL_SCHEMAS);
        const auto table = (TableName ? toUTF8(TableName, NameLength3) : "%");
        const auto column = (ColumnName ? toUTF8(ColumnName, NameLength4) : "%");

        // N.B.: here, an empty 'catalog', 'schema', 'table', or 'column' variable would mean, that an empty string
        // has been supplied, not a nullptr. In case of nullptr, it would contain "%".
        std::stringstream query;
        query << "SELECT "
            "cast(database, 'Nullable(String)') AS TABLE_CAT, "
            "cast('', 'Nullable(String)') AS TABLE_SCHEM, "
            "cast(table, 'String') AS TABLE_NAME, "
            "cast(name, 'String') AS COLUMN_NAME, "
            "cast(0, 'Int16') AS DATA_TYPE, "
            "cast(type, 'String') AS TYPE_NAME, "
            "cast(NULL, 'Nullable(Int32)') AS COLUMN_SIZE, "
            "cast(0, 'Nullable(Int32)') AS BUFFER_LENGTH, "
            "cast(NULL, 'Nullable(Int16)') AS DECIMAL_DIGITS, "
            "cast(NULL, 'Nullable(Int16)') AS NUM_PREC_RADIX, "
            "cast(0, 'Int16') AS NULLABLE, "
            "cast(NULL, 'Nullable(String)') AS REMARKS, "
            "cast(NULL, 'Nullable(String)') AS COLUMN_DEF, "
            "cast(0, 'Int16') AS SQL_DATA_TYPE, "
            "cast(NULL, 'Nullable(Int16)') AS SQL_DATETIME_SUB, "
            "cast(0, 'Nullable(Int32)') AS CHAR_OCTET_LENGTH, "
            "cast(position, 'Int32') AS ORDINAL_POSITION, "
            "cast(NULL, 'Nullable(String)') AS IS_NULLABLE "
            "FROM system.columns "
            "WHERE (1 == 1)";

        // Completely omit the condition part of the query, if the value of SQL_ATTR_METADATA_ID is SQL_TRUE
        // (i.e., values for the components are not patterns), and the component hasn't been supplied at all
        // (i.e. is nullptr; note, that actual empty strings are considered "supplied".)

        const auto is_pattern = (statement.getParent().getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE) != SQL_TRUE);

        // TODO: Use of coalesce() is a workaround here. Review.

        // Note, that 'catalog' variable will be set to "%" above (or to the connected database name), even if CatalogName == nullptr.
        if (is_pattern) {
            if (!isMatchAnythingCatalogFnPatternArg(catalog))
                query << " AND isNotNull(TABLE_CAT) AND coalesce(TABLE_CAT, '') LIKE '" << escapeForSQL(catalog) << "'";
        }
        else if (CatalogName) {
            query << " AND isNotNull(TABLE_CAT) AND coalesce(TABLE_CAT, '') == '" << escapeForSQL(catalog) << "'";
        }

        // FIXME(slabko): This does not make any sense, TABLE_SCHEM is always ''
        // Note, that 'schema' variable will be set to "%" above, even if SchemaName == nullptr.
        if (is_pattern) {
            if (!isMatchAnythingCatalogFnPatternArg(schema))
                query << " AND isNotNull(TABLE_SCHEM) AND coalesce(TABLE_SCHEM, '') LIKE '" << escapeForSQL(schema) << "'";
        }
        else if (SchemaName) {
            query << " AND isNotNull(TABLE_SCHEM) AND coalesce(TABLE_SCHEM, '') == '" << escapeForSQL(schema) << "'";
        }

        // Note, that 'table' variable will be set to "%" above, even if TableName == nullptr.
        if (is_pattern) {
            if (!isMatchAnythingCatalogFnPatternArg(table))
                query << " AND TABLE_NAME LIKE '" << escapeForSQL(table) << "'";
        }
        else if (TableName) {
            query << " AND TABLE_NAME == '" << escapeForSQL(table) << "'";
        }

        // Note, that 'column' variable will be set to "%" above, even if ColumnName == nullptr.
        if (is_pattern) {
            if (!isMatchAnythingCatalogFnPatternArg(column))
                query << " AND COLUMN_NAME LIKE '" << escapeForSQL(column) << "'";
        }
        else if (ColumnName) {
            query << " AND COLUMN_NAME == '" << escapeForSQL(column) << "'";
        }

        query << " ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION";
        statement.executeQuery(query.str(), std::make_unique<SQLColumnsResultSetMutator>(statement));

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetTypeInfo)(
    SQLHSTMT statement_handle,
    SQLSMALLINT type
) {
    LOG(__FUNCTION__ << "(type = " << type << ")");

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) {
        std::stringstream query;
        query << "SELECT * FROM (";

        // Power BI requires a UTF-16 compatible type to pass string-based parameters.
        // However, we do not use UTF-16 internally, and therefore do not declare such a type
        // in the TypeInfoCatalog. Without it, Power BI cannot bind its input parameters,
        // for example, in the WHERE clause.
        // To solve this problem, we suggest that Power BI use the same `String` type for both
        // UTF-16 and UTF-8. This way, Power BI will attempt to bind its input parameters as
        // `SQL_WVARCHAR`, and the driver will convert them to UTF-8 as needed.
        // By declaring this type only here, we also avoid cluttering the TypeInfoCatalog.
        std::vector<TypeInfo> types(std::cbegin(TypeInfoCatalog::Types), std::cend(TypeInfoCatalog::Types));
        auto string_u16 = TypeInfoCatalog::Types[DataSourceTypeIdIndex(DataSourceTypeId::String)];
        string_u16.data_type = SQL_WVARCHAR;
        types.push_back(string_u16);

        bool first = true;
        for (const auto & info : types) {
            if (type != SQL_ALL_TYPES && type != info.data_type)
                continue;

            // TODO(slabko): DecimalXXX will be removed in near future, only plain Decimal will remain.
            // For now we do not even want the clients to know of DecimalXXX existence.
            // This piece of code should be deleted when deleting DecimalXXX.
            if (info.isFixedPrecisionType() && info.type_id != DataSourceTypeId::Decimal)
                continue;

            if (!first)
                query << " UNION ALL ";
            first = false;

            query << "SELECT "
                "cast(" << toSqlQueryValue(info.type_name) << ", 'String') AS TYPE_NAME, "
                "cast(" << toSqlQueryValue(info.data_type) << ", 'Int16') AS DATA_TYPE, "
                "cast(" << toSqlQueryValue(info.column_size) << ", 'Nullable(Int32)') AS COLUMN_SIZE, "
                "cast(" << toSqlQueryValue(info.literal_wrapper) << ", 'Nullable(String)') AS LITERAL_PREFIX, "
                "cast(" << toSqlQueryValue(info.literal_wrapper) << ", 'Nullable(String)') AS LITERAL_SUFFIX, "
                "cast(" << toSqlQueryValue(info.create_params) << ", 'Nullable(String)') AS CREATE_PARAMS, "
                "cast(" << toSqlQueryValue(SQL_NULLABLE) << ", 'Int16') AS NULLABLE, "
                "cast(" << toSqlQueryValue(SQL_TRUE) << ", 'Int16') AS CASE_SENSITIVE, "
                "cast(" << toSqlQueryValue(SQL_SEARCHABLE) << ", 'Int16') AS SEARCHABLE, "
                "cast(" << toSqlQueryValue(info.unsigned_attribute) << ", 'Nullable(Int16)') AS UNSIGNED_ATTRIBUTE, "
                "cast(" << toSqlQueryValue(SQL_FALSE) << ", 'Int16') AS FIXED_PREC_SCALE, "
                "cast(NULL, 'Nullable(Int16)') AS AUTO_UNIQUE_VALUE, "
                "cast(NULL, 'Nullable(String)') AS LOCAL_TYPE_NAME, "
                "cast(" << toSqlQueryValue(info.minimum_scale) << ", 'Nullable(Int16)') AS MINIMUM_SCALE, "
                "cast(" << toSqlQueryValue(info.maximum_scale) << ", 'Nullable(Int16)') AS MAXIMUM_SCALE, "
                "cast(" << toSqlQueryValue(info.sql_data_type) << ", 'Int16') AS SQL_DATA_TYPE, "
                "cast(" << toSqlQueryValue(info.sql_datetime_sub) << ", 'Nullable(Int16)') AS SQL_DATETIME_SUB, "
                "cast(" << toSqlQueryValue(info.num_prec_radix) << ", 'Nullable(Int32)') AS NUM_PREC_RADIX, "
                "cast(NULL, 'Nullable(Int16)') AS INTERVAL_PRECISION";
        }

        // TODO (slabko): From ODBC documentation for SQLGetTypeInfo:
        // "SQLGetTypeInfo returns the results as a standard result set,
        // ordered by DATA_TYPE **and then by how closely the data type maps
        // to the corresponding ODBC SQL data type**. Data types defined
        // by the data source take precedence over user-defined data types."
        // This, however, does not order by how closely the data type maps
        // to the data type passed as parameter to SQLGetTypeInfo.
        query << ") ORDER BY DATA_TYPE";

        if (first)
            query.str("SELECT 1 WHERE 0");

        statement.executeQuery(query.str());
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLNumParams)(
    SQLHSTMT        StatementHandle,
    SQLSMALLINT *   ParameterCountPtr
) {
    LOG(__FUNCTION__);
    return impl::NumParams(
        StatementHandle,
        ParameterCountPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLNativeSql)(HDBC connection_handle,
    SQLTCHAR * query,
    SQLINTEGER query_length,
    SQLTCHAR * out_query,
    SQLINTEGER out_query_max_length,
    SQLINTEGER * out_query_length) {
    LOG(__FUNCTION__);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, [&](Connection & connection) {
        std::string query_str = toUTF8(query, query_length);
        return fillOutputString<PTChar>(query_str, out_query, out_query_max_length, out_query_length, false);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCloseCursor)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, [&](Statement & statement) -> SQLRETURN {
        statement.closeCursor();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLBrowseConnect)(HDBC connection_handle,
    SQLTCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLTCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCancel)(
    SQLHSTMT     StatementHandle
) {
    auto func = [&] (Statement & statement) {
        statement.closeCursor();
        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetCursorName)(
    HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT BufferLength, SQLSMALLINT * NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLGetFunctions)(HDBC connection_handle, SQLUSMALLINT FunctionId, SQLUSMALLINT * Supported) {
    LOG(__FUNCTION__ << ":" << __LINE__ << " " << " id=" << FunctionId << " ptr=" << Supported);

#define SET_EXISTS(x) Supported[(x) >> 4] |= (1 << ((x)&0xF))
// #define CLR_EXISTS(x) Supported[(x) >> 4] &= ~(1 << ((x) & 0xF))

    auto func = [&] (Connection & connection) -> SQLRETURN {
        if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
            memset(Supported, 0, sizeof(Supported[0]) * SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);

            SET_EXISTS(SQL_API_SQLALLOCHANDLE);
            SET_EXISTS(SQL_API_SQLBINDCOL);
            SET_EXISTS(SQL_API_SQLBINDPARAMETER);
#if defined(WORKAROUND_ENABLE_DEFINE_SQLBindParam)
            SET_EXISTS(SQL_API_SQLBINDPARAM);
#endif
            //SET_EXISTS(SQL_API_SQLBROWSECONNECT);
            //SET_EXISTS(SQL_API_SQLBULKOPERATIONS);
            SET_EXISTS(SQL_API_SQLCANCEL);
            //SET_EXISTS(SQL_API_SQLCANCELHANDLE);
            SET_EXISTS(SQL_API_SQLCLOSECURSOR);
            SET_EXISTS(SQL_API_SQLCOLATTRIBUTE);
            //SET_EXISTS(SQL_API_SQLCOLUMNPRIVILEGES);
            SET_EXISTS(SQL_API_SQLCOLUMNS);
            //SET_EXISTS(SQL_API_SQLCOMPLETEASYNC);
            SET_EXISTS(SQL_API_SQLCONNECT);
            SET_EXISTS(SQL_API_SQLCOPYDESC);
            SET_EXISTS(SQL_API_SQLDESCRIBECOL);
            SET_EXISTS(SQL_API_SQLDESCRIBEPARAM);
            SET_EXISTS(SQL_API_SQLDISCONNECT);
            SET_EXISTS(SQL_API_SQLDRIVERCONNECT);
            SET_EXISTS(SQL_API_SQLENDTRAN);
            SET_EXISTS(SQL_API_SQLEXECDIRECT);
            SET_EXISTS(SQL_API_SQLEXECUTE);
            //SET_EXISTS(SQL_API_SQLEXTENDEDFETCH);
            SET_EXISTS(SQL_API_SQLFETCH);
            SET_EXISTS(SQL_API_SQLFETCHSCROLL);
            //SET_EXISTS(SQL_API_SQLFOREIGNKEYS);
            SET_EXISTS(SQL_API_SQLFREEHANDLE);
            SET_EXISTS(SQL_API_SQLFREESTMT);
            SET_EXISTS(SQL_API_SQLGETCONNECTATTR);
            //SET_EXISTS(SQL_API_SQLGETCURSORNAME);
            SET_EXISTS(SQL_API_SQLGETDATA);
            SET_EXISTS(SQL_API_SQLGETDESCFIELD);
            SET_EXISTS(SQL_API_SQLGETDESCREC);
            SET_EXISTS(SQL_API_SQLGETDIAGFIELD);
            SET_EXISTS(SQL_API_SQLGETDIAGREC);
            SET_EXISTS(SQL_API_SQLGETENVATTR);
            SET_EXISTS(SQL_API_SQLGETFUNCTIONS);
            SET_EXISTS(SQL_API_SQLGETINFO);
            SET_EXISTS(SQL_API_SQLGETSTMTATTR);
            SET_EXISTS(SQL_API_SQLGETTYPEINFO);
            SET_EXISTS(SQL_API_SQLMORERESULTS);
            SET_EXISTS(SQL_API_SQLNATIVESQL);
            SET_EXISTS(SQL_API_SQLNUMPARAMS);
            SET_EXISTS(SQL_API_SQLNUMRESULTCOLS);
            //SET_EXISTS(SQL_API_SQLPARAMDATA);
            SET_EXISTS(SQL_API_SQLPREPARE);
            //SET_EXISTS(SQL_API_SQLPRIMARYKEYS);
            //SET_EXISTS(SQL_API_SQLPROCEDURECOLUMNS);
            //SET_EXISTS(SQL_API_SQLPROCEDURES);
            //SET_EXISTS(SQL_API_SQLPUTDATA);
            SET_EXISTS(SQL_API_SQLROWCOUNT);
            SET_EXISTS(SQL_API_SQLSETCONNECTATTR);
            //SET_EXISTS(SQL_API_SQLSETCURSORNAME);
            SET_EXISTS(SQL_API_SQLSETDESCFIELD);
            SET_EXISTS(SQL_API_SQLSETDESCREC);
            SET_EXISTS(SQL_API_SQLSETENVATTR);
            //SET_EXISTS(SQL_API_SQLSETPOS);
            SET_EXISTS(SQL_API_SQLSETSTMTATTR);
            //SET_EXISTS(SQL_API_SQLSPECIALCOLUMNS);
            //SET_EXISTS(SQL_API_SQLSTATISTICS);
            //SET_EXISTS(SQL_API_SQLTABLEPRIVILEGES);
            SET_EXISTS(SQL_API_SQLTABLES);

            return SQL_SUCCESS;
        } else if (FunctionId == SQL_API_ALL_FUNCTIONS) {
            //memset(Supported, 0, sizeof(Supported[0]) * 100);
            return SQL_ERROR;
        } else {
/*
		switch (FunctionId) {
			case SQL_API_SQLBINDCOL:
				*Supported = SQL_TRUE;
				break;
			default:
				*Supported = SQL_FALSE;
				break;
		}
*/
            return SQL_ERROR;
        }

        return SQL_ERROR;
    };

#undef SET_EXISTS
// #undef CLR_EXISTS

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLParamData)(HSTMT StatementHandle, PTR * Value) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLPutData)(HSTMT StatementHandle, PTR Data, SQLLEN StrLen_or_Ind) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetCursorName)(HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSpecialColumns)(HSTMT StatementHandle,
    SQLUSMALLINT IdentifierType,
    SQLTCHAR * CatalogName,
    SQLSMALLINT NameLength1,
    SQLTCHAR * SchemaName,
    SQLSMALLINT NameLength2,
    SQLTCHAR * TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Scope,
    SQLUSMALLINT Nullable) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLStatistics)(HSTMT StatementHandle,
    SQLTCHAR * CatalogName,
    SQLSMALLINT NameLength1,
    SQLTCHAR * SchemaName,
    SQLSMALLINT NameLength2,
    SQLTCHAR * TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Unique,
    SQLUSMALLINT Reserved) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColumnPrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLTCHAR * szColumnName,
    SQLSMALLINT cbColumnName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDescribeParam)(
    SQLHSTMT        StatementHandle,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT *   DataTypePtr,
    SQLULEN *       ParameterSizePtr,
    SQLSMALLINT *   DecimalDigitsPtr,
    SQLSMALLINT *   NullablePtr
) {
    LOG(__FUNCTION__);
    return impl::DescribeParam(
        StatementHandle,
        ParameterNumber,
        DataTypePtr,
        ParameterSizePtr,
        DecimalDigitsPtr,
        NullablePtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLExtendedFetch)(
    SQLHSTMT         StatementHandle,
    SQLUSMALLINT     FetchOrientation,
    SQLLEN           FetchOffset,
    SQLULEN *        RowCountPtr,
    SQLUSMALLINT *   RowStatusArray
) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLForeignKeys)(HSTMT hstmt,
    SQLTCHAR * szPkCatalogName,
    SQLSMALLINT cbPkCatalogName,
    SQLTCHAR * szPkSchemaName,
    SQLSMALLINT cbPkSchemaName,
    SQLTCHAR * szPkTableName,
    SQLSMALLINT cbPkTableName,
    SQLTCHAR * szFkCatalogName,
    SQLSMALLINT cbFkCatalogName,
    SQLTCHAR * szFkSchemaName,
    SQLSMALLINT cbFkSchemaName,
    SQLTCHAR * szFkTableName,
    SQLSMALLINT cbFkTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLPrimaryKeys)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLProcedureColumns)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName,
    SQLTCHAR * szColumnName,
    SQLSMALLINT cbColumnName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLProcedures)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLSetPos)(HSTMT hstmt, SQLSETPOSIROW irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLTablePrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBindParameter)(
    SQLHSTMT        StatementHandle,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT     InputOutputType,
    SQLSMALLINT     ValueType,
    SQLSMALLINT     ParameterType,
    SQLULEN         ColumnSize,
    SQLSMALLINT     DecimalDigits,
    SQLPOINTER      ParameterValuePtr,
    SQLLEN          BufferLength,
    SQLLEN *        StrLen_or_IndPtr
) {
    LOG(__FUNCTION__);
    return impl::BindParameter(
        StatementHandle,
        ParameterNumber,
        InputOutputType,
        ValueType,
        ParameterType,
        ColumnSize,
        DecimalDigits,
        ParameterValuePtr,
        BufferLength,
        StrLen_or_IndPtr
    );
}

// Workaround for iODBC: when driver is in ODBCv3 mode, iODBC still probes for SQLBindParam() even though SQLBindParameter() is found.
// It finds SQLBindParam(), but the actual functions pointer points to an fallback implementation of the Driver Manager itself (due to symbol resolution logic).
// Moreover, the code still calls SQLBindParam() instead of SQLBindParameter(), causing invalid handle error due to masked handler.
// TODO: review and report an error. Even if there is a problem in linkage, the login behind iODBC still seems to be faulty.
// See SQLBindParameter_Internal() function defined in https://github.com/openlink/iODBC/blob/master/iodbc/prepare.c
#if defined(WORKAROUND_ENABLE_DEFINE_SQLBindParam)
SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBindParam)(
    SQLHSTMT        StatementHandle,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT     ValueType,
    SQLSMALLINT     ParameterType,
    SQLULEN         ColumnSize,
    SQLSMALLINT     DecimalDigits,
    SQLPOINTER      ParameterValuePtr,
    SQLLEN *        StrLen_or_IndPtr
) {
    LOG(__FUNCTION__);
    return impl::BindParameter(
        StatementHandle,
        ParameterNumber,
        SQL_PARAM_INPUT,
        ValueType,
        ParameterType,
        ColumnSize,
        DecimalDigits,
        ParameterValuePtr,
        SQL_MAX_OPTION_STRING_LENGTH,
        StrLen_or_IndPtr
    );
}
#endif

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBulkOperations)(
    SQLHSTMT         StatementHandle,
    SQLSMALLINT      Operation
) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCancelHandle)(SQLSMALLINT HandleType, SQLHANDLE Handle) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCompleteAsync)(SQLSMALLINT HandleType, SQLHANDLE Handle, RETCODE * AsyncRetCodePtr) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLEndTran)(
    SQLSMALLINT     HandleType,
    SQLHANDLE       Handle,
    SQLSMALLINT     CompletionType
) {
    LOG(__FUNCTION__);
    return impl::EndTran(
        HandleType,
        Handle,
        CompletionType
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDescField)(
    SQLHDESC        DescriptorHandle,
    SQLSMALLINT     RecNumber,
    SQLSMALLINT     FieldIdentifier,
    SQLPOINTER      ValuePtr,
    SQLINTEGER      BufferLength,
    SQLINTEGER *    StringLengthPtr
) {
    LOG(__FUNCTION__);
    return impl::GetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDescRec)(
    SQLHDESC        DescriptorHandle,
    SQLSMALLINT     RecNumber,
    SQLTCHAR *      Name,
    SQLSMALLINT     BufferLength,
    SQLSMALLINT *   StringLengthPtr,
    SQLSMALLINT *   TypePtr,
    SQLSMALLINT *   SubTypePtr,
    SQLLEN *        LengthPtr,
    SQLSMALLINT *   PrecisionPtr,
    SQLSMALLINT *   ScalePtr,
    SQLSMALLINT *   NullablePtr
) {
    LOG(__FUNCTION__);
    return impl::GetDescRec(
        DescriptorHandle,
        RecNumber,
        Name,
        BufferLength,
        StringLengthPtr,
        TypePtr,
        SubTypePtr,
        LengthPtr,
        PrecisionPtr,
        ScalePtr,
        NullablePtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetDescField)(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   FieldIdentifier,
    SQLPOINTER    ValuePtr,
    SQLINTEGER    BufferLength
) {
    LOG(__FUNCTION__);
    return impl::SetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLSetDescRec)(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   Type,
    SQLSMALLINT   SubType,
    SQLLEN        Length,
    SQLSMALLINT   Precision,
    SQLSMALLINT   Scale,
    SQLPOINTER    DataPtr,
    SQLLEN *      StringLengthPtr,
    SQLLEN *      IndicatorPtr
) {
    LOG(__FUNCTION__);
    return impl::SetDescRec(
        DescriptorHandle,
        RecNumber,
        Type,
        SubType,
        Length,
        Precision,
        Scale,
        DataPtr,
        StringLengthPtr,
        IndicatorPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCopyDesc)(
    SQLHDESC     SourceDescHandle,
    SQLHDESC     TargetDescHandle
) {
    LOG(__FUNCTION__);
    return impl::CopyDesc(
        SourceDescHandle,
        TargetDescHandle
    );
}

/*
 *	This function is used to cause the Driver Manager to
 *	call functions by number rather than name, which is faster.
 *	The ordinal value of this function must be 199 to have the
 *	Driver Manager do this.  Also, the ordinal values of the
 *	functions must match the value of fFunction in SQLGetFunctions()
 *
 *	EDIT: not relevant for 3.x drivers. Currently, used for testing dynamic loading only.
 */
SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDummyOrdinal)(void) {
    return SQL_SUCCESS;
}

} // extern "C"
