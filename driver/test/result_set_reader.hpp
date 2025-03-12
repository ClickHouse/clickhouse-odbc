#include <type_traits>

#include "driver/test/client_utils.h"

// This trait is a good candidate to move to type_info.h
// however, I'll keep it here until it is needed in type_info.h.
template <typename SqlType>
struct IsTrivialSqlType : std::false_type{};

template <> struct IsTrivialSqlType<SQLINTEGER> : std::true_type{};
template <> struct IsTrivialSqlType<SQLSMALLINT> : std::true_type{};
template <> struct IsTrivialSqlType<SQLUSMALLINT> : std::true_type{};
template <> struct IsTrivialSqlType<SQLBIGINT> : std::true_type{};
template <> struct IsTrivialSqlType<SQLUBIGINT> : std::true_type{};
template <> struct IsTrivialSqlType<SQLREAL> : std::true_type{};
template <> struct IsTrivialSqlType<SQLDOUBLE> : std::true_type{};
template <> struct IsTrivialSqlType<SQL_DATE_STRUCT> : std::true_type{};
template <> struct IsTrivialSqlType<SQL_TIMESTAMP_STRUCT> : std::true_type{};

class ResultSetReader
{
private:
    SQLHSTMT stmt;
    std::unordered_map<std::string, SQLSMALLINT> columns_indices;

    // Size of the buffer when calling SQLGetData for strings
    // should work in most cases, in special cases use setMaxStringSize()
    size_t max_string_size = 1024;

public:
    explicit ResultSetReader(SQLHSTMT stmt) : stmt{stmt}
    {
        SQLSMALLINT num_columns{};
        ODBC_CALL_ON_STMT_THROW(stmt, SQLNumResultCols(stmt, &num_columns));

        SQLSMALLINT name_length = 0;
        SQLSMALLINT nullable = 0;

        // TODO(slabko):             IMPORTANT NOTE:
        // The standard does not allow std::basic_string specialization for unsigned
        // types. It's a miracle that this works in the current version of libc++,
        // and it will definitely break on update. However, the code is littered
        // with such specializations of std::basic_strings, and this should be solved
        // consistently across the whole code base. For now, I'll stick to this common,
        // yet incorrect, approach.
        // For reference: https://reviews.llvm.org/D138307
        std::basic_string<SQLTCHAR> input_name(256, '\0');
        for (SQLSMALLINT idx = 1; idx <= num_columns; ++idx) {
            ODBC_CALL_ON_STMT_THROW(stmt, SQLDescribeCol(
                stmt,
                idx,
                input_name.data(),
                static_cast<SQLSMALLINT>(input_name.size()),
                &name_length,
                nullptr,
                nullptr,
                nullptr,
                &nullable));
            std::string name(input_name.begin(), input_name.begin() + name_length);
            assert(!columns_indices.contains(name)
               && "two columns with the same name in the dataset");
            columns_indices[name] = idx;
        }
    }

    void setMaxStringSize(size_t size)
    {
        max_string_size = size;
    }

    bool fetch()
    {
        auto rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA)
            return false;

        if (rc != SQL_SUCCESS)
            // Normally SQL_SUCCESS_WITH_INFO is not an error
            // but in tests if we should not expect it,
            // so it is better to fail and show the info
            ODBC_CALL_THROW(stmt, SQL_HANDLE_STMT, rc);

        return true;
    }

    template <typename SqlType>
    std::optional<SqlType> getData(const std::string& column);

    template <typename SqlType>
        requires IsTrivialSqlType<SqlType>::value
    std::optional<SqlType> getData(const std::string& column)
    {
        SqlType buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(stmt, SQLGetData(
            stmt,
            columns_indices.at(column),
            getCTypeFor<SqlType>(),
            &buffer,
            sizeof(SqlType),
            &indicator
        ));

        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }

        return buffer;
    }

    template <>
    std::optional<std::string> getData(const std::string& column)
    {
        std::string buffer(max_string_size, '\0');
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(stmt, SQLGetData(
            stmt,
            columns_indices.at(column),
            SQL_C_CHAR,
            buffer.data(),
            buffer.size(),
            &indicator
        ));

        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }

        assert(indicator >= 0 && "cannot read size from a negative indicator");
        buffer.resize(indicator);
        return buffer;
    }

    // Feel free to extend the class to support the types you need
};
