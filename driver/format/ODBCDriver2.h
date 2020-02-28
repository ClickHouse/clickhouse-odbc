#pragma once

#include "driver/platform/platform.h"
#include "driver/result_set.h"

// Implementation of ResultSet for ODBCDriver2 wire format of ClickHouse.
class ODBCDriver2ResultSet
    : public ResultSet
{
public:
    explicit ODBCDriver2ResultSet(std::istream & stream, std::unique_ptr<ResultMutator> && mutator);
    virtual ~ODBCDriver2ResultSet() override = default;

protected:
    virtual bool readNextRow(Row & row) override;

private:
    void readSize(std::int32_t & dest);

    void readValue(std::string & dest, bool * is_null = nullptr);

    void readValue(Field & dest, ColumnInfo & column_info);

    template <typename T>
    void readValueAs(std::string & src, Field & dest, ColumnInfo & column_info) {
        T value;
        readValue(src, value, column_info);
        dest.data = std::move(value);
    }

    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Date        > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::DateTime    > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Decimal     > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Decimal32   > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Decimal64   > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Decimal128  > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::FixedString > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Float32     > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Float64     > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Int8        > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Int16       > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Int32       > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Int64       > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::Nothing     > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::String      > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::UInt8       > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::UInt16      > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::UInt32      > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::UInt64      > & dest, ColumnInfo & column_info);
    void readValue(std::string & src, DataSourceType< DataSourceTypeId::UUID        > & dest, ColumnInfo & column_info);

    template <typename T>
    void readValue(std::string & src, T & dest, ColumnInfo & column_info) {
        throw std::runtime_error("Unable to decode value of type '" + column_info.type + "'");
    }
};

class ODBCDriver2ResultReader
    : public ResultReader
{
public:
    explicit ODBCDriver2ResultReader(std::istream & stream, std::unique_ptr<ResultMutator> && mutator);
    virtual ~ODBCDriver2ResultReader() override = default;

    virtual bool advanceToNextResultSet() override;
};
