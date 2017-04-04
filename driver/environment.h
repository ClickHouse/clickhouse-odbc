#pragma once

#include "diagnostics.h"

#include <stdio.h>

#include <map>
#include <stdexcept>

struct TypeInfo
{
    std::string sql_type_name;
    bool is_unsigned;
    SQLSMALLINT sql_type;
    size_t column_size;
};


struct Environment
{
    Environment()
    {
#if defined (_unix_)
        std::string stderr_path = "/tmp/clickhouse-odbc-stderr";
        if (!freopen(stderr_path.c_str(), "a+", stderr))
            throw std::logic_error("Cannot freopen stderr.");
#endif
    }

    const std::map<std::string, TypeInfo> types_info =
    {
		{"UInt8",       TypeInfo{"TINYINT",   true,    SQL_TINYINT,			3}},
		{"UInt16",      TypeInfo{"SMALLINT",  true,    SQL_SMALLINT,		5}},
        {"UInt32",      TypeInfo{"INT",       true,    SQL_INTEGER,			10}},
        {"UInt64",      TypeInfo{"BIGINT",    true,    SQL_BIGINT,			19}},
        {"Int8",        TypeInfo{"TINYINT",   false,   SQL_TINYINT,			3}},
        {"Int16",       TypeInfo{"SMALLINT",  false,   SQL_SMALLINT,		5}},
        {"Int32",       TypeInfo{"INT",       false,   SQL_INTEGER,			10}},
        {"Int64",       TypeInfo{"BIGINT",    false,   SQL_BIGINT,			20}},
        {"Float32",     TypeInfo{"REAL",      false,   SQL_REAL,			7}},
        {"Float64",     TypeInfo{"DOUBLE",    false,   SQL_DOUBLE,			15}},
        {"String",      TypeInfo{"TEXT",      true,    SQL_VARCHAR,			0xFFFFFF}},
        {"FixedString", TypeInfo{"TEXT",      true,    SQL_VARCHAR,			0xFFFFFF}},
        {"Date",        TypeInfo{"DATE",      true,    SQL_TYPE_DATE,		10}},
        {"DateTime",    TypeInfo{"TIMESTAMP", true,    SQL_TYPE_TIMESTAMP,	19}},
        {"Array",       TypeInfo{"TEXT",      true,    SQL_VARCHAR,			0xFFFFFF}},
    };

/*  Poco::UTF8Encoding utf8;
    Poco::UTF16Encoding utf16;
    Poco::TextConverter converter_utf8_to_utf16 {utf8, utf16};*/

    int odbc_version = SQL_OV_ODBC3;
    DiagnosticRecord diagnostic_record;
};
