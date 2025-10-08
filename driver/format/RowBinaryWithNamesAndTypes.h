#pragma once

#include "driver/platform/platform.h"
#include "driver/result_set.h"

// Implementation of ResultSet for RowBinaryWithNamesAndTypes wire format of ClickHouse.
class RowBinaryWithNamesAndTypesResultSet
    : public ResultSet
{
public:
    explicit RowBinaryWithNamesAndTypesResultSet(const std::string & timezone, AmortizedIStreamReader & stream, std::unique_ptr<ResultMutator> && mutator);
    virtual ~RowBinaryWithNamesAndTypesResultSet() override = default;

protected:
    virtual bool readNextRow(Row & row) override;

private:
    void readSize(std::uint64_t & dest);

    void readValue(bool & dest);
    void readValue(std::string & dest);
    void readValue(std::string & dest, const std::uint64_t size);

    template <typename T>
    void readPOD(T & dest) {
        stream.read(reinterpret_cast<char *>(&dest), sizeof(T));
    }

    void readValue(Field & dest, ColumnInfo & column_info);

    template <typename T>
    void readValueUsing(T && value, Field & dest, ColumnInfo & column_info) {
        readValue(value, column_info);
        dest.data = std::forward<T>(value);
    }

    template <typename T>
    void readValueAs(Field & dest, ColumnInfo & column_info) {
        return readValueUsing(T(), dest, column_info);
    }

    void readValue(WireTypeDateAsInt & dest, ColumnInfo & column_info);
    void readValue(WireTypeDateTimeAsInt & dest, ColumnInfo & column_info);
    void readValue(WireTypeDateTime64AsInt & dest, ColumnInfo & column_info);

    void readValue(DataSourceType< DataSourceTypeId::Date        > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::DateTime    > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::DateTime64  > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Decimal     > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Decimal32   > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Decimal64   > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Decimal128  > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::FixedString > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Float32     > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Float64     > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Int8        > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Int16       > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Int32       > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Int64       > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::Nothing     > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::String      > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::UInt8       > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::UInt16      > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::UInt32      > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::UInt64      > & dest, ColumnInfo & column_info);
    void readValue(DataSourceType< DataSourceTypeId::UUID        > & dest, ColumnInfo & column_info);

    template <typename T>
    void readValue(T & dest, ColumnInfo & column_info) {
        throw std::runtime_error("Unable to decode value of type '" + column_info.type + "'");
    }
};

class RowBinaryWithNamesAndTypesResultReader
    : public ResultReader
{
public:
    explicit RowBinaryWithNamesAndTypesResultReader(const std::string & timezone, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator);
    explicit RowBinaryWithNamesAndTypesResultReader(const std::string & timezone, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator, std::unique_ptr<std::istream> && inflating_input_stream);
    virtual ~RowBinaryWithNamesAndTypesResultReader() override = default;

    virtual bool advanceToNextResultSet() override;
};
