#include "result_set.h"

#include "statement.h"

uint64_t Field::getUInt() const {
    try {
        return std::stoull(data);
    } catch (std::exception & e) {
        throw std::runtime_error("Cannot interpret '" + data + "' as uint64: " + e.what());
    }
}

int64_t Field::getInt() const {
    try {
        return std::stoll(data);
    } catch (std::exception & e) {
        throw std::runtime_error("Cannot interpret '" + data + "' as int64: " + e.what());
    }
}

float Field::getFloat() const {
    try {
        return std::stof(data);
    } catch (std::exception & e) {
        throw std::runtime_error("Cannot interpret '" + data + "' as float: " + e.what());
    }
}

double Field::getDouble() const {
    try {
        return std::stod(data);
    } catch (std::exception & e) {
        throw std::runtime_error("Cannot interpret '" + data + "' as double: " + e.what());
    }
}

SQLGUID Field::getGUID() const {
    unsigned int Data1 = 0;
    unsigned int Data2 = 0;
    unsigned int Data3 = 0;
    unsigned int Data4[8] = { 0 };
    char guard = '\0';

    const auto read = std::sscanf(data.c_str(), "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x%c",
        &Data1, &Data2, &Data3,
        &Data4[0], &Data4[1], &Data4[2], &Data4[3],
        &Data4[4], &Data4[5], &Data4[6], &Data4[7],
        &guard
    );

    if (read != 11) // All 'DataN' must be successfully read, but not the 'guard'.
        throw std::runtime_error("Cannot interpret '" + data + "' as GUID");

    SQLGUID res;

    res.Data1 = Data1;
    res.Data2 = Data2;
    res.Data3 = Data3;
    std::copy(std::begin(Data4), std::end(Data4), std::begin(res.Data4));

    return res;
}

SQL_NUMERIC_STRUCT Field::getNumeric(const std::int16_t precision, const std::int16_t scale) const {
    if (precision < 1 || precision < scale)
        throw std::runtime_error("Bad Numeric specification");

    SQL_NUMERIC_STRUCT res;
    res.precision = precision;
    res.scale = scale;
    res.sign = 1;
    std::fill(std::begin(res.val), std::end(res.val), 0);

    numeric_uint_container_t bigint = 0;

    constexpr auto bigint_max = std::numeric_limits<decltype(bigint)>::max();
    constexpr std::uint32_t dec_mult = 10;
    constexpr std::uint32_t byte_mult = 1 << 8;

    std::size_t left_n = 0;
    std::size_t right_n = 0;
    bool sign_met = false;
    bool dot_met = false;
    bool dig_met = false;

    for (auto ch : data) {
        switch (ch) {
            case '+':
            case '-': {
                if (sign_met || dot_met || dig_met)
                    throw std::runtime_error("Cannot interpret '" + data + "' as Numeric");

                if (ch == '-')
                    res.sign = 0;

                sign_met = true;
                break;
            }

            case '.': {
                if (dot_met || dig_met)
                    throw std::runtime_error("Cannot interpret '" + data + "' as Numeric");

                dot_met = true;
                break;
            }

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                const std::uint32_t next_dec_dig = static_cast<unsigned char>(ch - '0');

                if (bigint != 0) {
                    if ((bigint_max / dec_mult) < bigint)
                        throw std::runtime_error("Cannot interpret '" + data + "' as Numeric: value is too big for internal representation");
                    bigint *= dec_mult;
                }

                if ((bigint_max - next_dec_dig) < bigint)
                    throw std::runtime_error("Cannot interpret '" + data + "' as Numeric: value is too big for internal representation");
                bigint += next_dec_dig;

                if (dot_met)
                    ++right_n;
                else
                    ++left_n;

                dig_met = true;
                break;
            }

            default:
                throw std::runtime_error("Cannot interpret '" + data + "' as Numeric");
        }
    }

    // TODO: add proper conversion/truncation?
    if (
        res.precision < (left_n + right_n) ||
        res.scale < right_n
    ) {
        throw std::runtime_error("Cannot fit source Numeric value '" + data + "' into destination Numeric specification");
    }

    if (bigint != 0) {
        for (; right_n < res.scale; ++right_n) {
            if ((bigint_max / dec_mult) < bigint)
                throw std::runtime_error("Cannot fit source Numeric value '" + data + "' into destination Numeric specification: value is too big for internal representation");
            bigint *= dec_mult;
        }
    }

    for (std::size_t i = 0; bigint > 0; ++i) {
        if (i >= lengthof(res.val))
            throw std::runtime_error("Cannot fit source Numeric value '" + data + "' into destination Numeric specification: value is too big for ODBC Numeric representation");

        res.val[i] = static_cast<std::uint32_t>(bigint % byte_mult);
        bigint /= byte_mult;
    }

    return res;
}

SQL_DATE_STRUCT Field::getDate() const {
    if (data.size() != 10)
        throw std::runtime_error("Cannot interpret '" + data + "' as Date");

    SQL_DATE_STRUCT res;
    res.year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
    res.month = (data[5] - '0') * 10 + (data[6] - '0');
    res.day = (data[8] - '0') * 10 + (data[9] - '0');

    normalizeDate(res);

    return res;
}

SQL_TIMESTAMP_STRUCT Field::getDateTime() const {
    SQL_TIMESTAMP_STRUCT res;

    if (data.size() == 10) {
        res.year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
        res.month = (data[5] - '0') * 10 + (data[6] - '0');
        res.day = (data[8] - '0') * 10 + (data[9] - '0');
        res.hour = 0;
        res.minute = 0;
        res.second = 0;
        res.fraction = 0;
    } else if (data.size() == 19) {
        res.year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
        res.month = (data[5] - '0') * 10 + (data[6] - '0');
        res.day = (data[8] - '0') * 10 + (data[9] - '0');
        res.hour = (data[11] - '0') * 10 + (data[12] - '0');
        res.minute = (data[14] - '0') * 10 + (data[15] - '0');
        res.second = (data[17] - '0') * 10 + (data[18] - '0');
        res.fraction = 0;
    } else {
        throw std::runtime_error("Cannot interpret '" + data + "' as DateTime");
    }

    normalizeDate(res);

    return res;
}

template <typename T>
void Field::normalizeDate(T & date) const {
    if (date.year == 0)
        date.year = 1970;
    if (date.month == 0)
        date.month = 1;
    if (date.day == 0)
        date.day = 1;
}

void assignTypeInfo(const TypeAst & ast, ColumnInfo * info) {
    if (ast.meta == TypeAst::Terminal) {
        info->type_without_parameters = ast.name;
        if (ast.elements.size() == 1)
            info->fixed_size = ast.elements.front().size;
    } else if (ast.meta == TypeAst::Nullable) {
        info->is_nullable = true;
        assignTypeInfo(ast.elements.front(), info);
    } else {
        // Interprete all unsupported types as String.
        info->type_without_parameters = "String";
    }
}

ResultSet::ResultSet(std::istream & in_, IResultMutatorPtr && mutator_)
    : in(in_)
    , mutator(std::move(mutator_))
{
    if (in.peek() == EOF) {
        finished = true;
        return;
    }

    int32_t num_header_rows = 0;
    readSize(in, num_header_rows);
    if (!num_header_rows)
        return;

    for (size_t row_n = 0; row_n < num_header_rows; ++row_n) {
        /// Title: number of columns, their names and types.
        int32_t num_columns = 0;
        readSize(in, num_columns);

        if (num_columns <= 1)
            return;

        std::string row_name;
        readString(in, row_name);
        --num_columns;

        if (row_name == "name") {
            columns_info.resize(num_columns);
            for (size_t i = 0; i < num_columns; ++i) {
                readString(in, columns_info[i].name);
            }
        } else if (row_name == "type") {
            columns_info.resize(num_columns);
            for (size_t i = 0; i < num_columns; ++i) {
                readString(in, columns_info[i].type);
                {
                    TypeAst ast;
                    if (TypeParser(columns_info[i].type).parse(&ast)) {
                        assignTypeInfo(ast, &columns_info[i]);
                    } else {
                        // Interprete all unknown types as String.
                        columns_info[i].type_without_parameters = "String";
                    }
                }
                LOG("Row " << i << " name=" << columns_info[i].name << " type=" << columns_info[i].type << " -> " << columns_info[i].type
                           << " typenoparams=" << columns_info[i].type_without_parameters << " fixedsize=" << columns_info[i].fixed_size);
            }

            // TODO: max_length

        } else {
            LOG("Unknown header " << row_name << "; Columns left: " << num_columns);
            for (size_t i = 0; i < num_columns; ++i) {
                std::string dummy;
                readString(in, dummy);
            }
        }
    }

    if (mutator)
        mutator->UpdateColumnInfo(&columns_info);

    prepareSomeRows();
}

size_t ResultSet::getNumColumns() const {
    return columns_info.size();
}

const ColumnInfo & ResultSet::getColumnInfo(size_t i) const {
    return columns_info.at(i);
}

bool ResultSet::hasCurrentRow() const {
    return current_row.isValid();
}

const Row & ResultSet::getCurrentRow() const {
    return current_row;
}

std::size_t ResultSet::getCurrentRowNum() const {
    return current_row_num;
}

bool ResultSet::advanceToNextRow() {
    if (endOfSet()) {
        current_row = Row{};
    }
    else {
        current_row = std::move(ready_raw_rows.front());
        ready_raw_rows.pop_front();
        ++current_row_num;

        if (mutator)
            mutator->UpdateRow(columns_info, &current_row);
    }

    return hasCurrentRow();
}

IResultMutatorPtr ResultSet::releaseMutator() {
    return std::move(mutator);
}

bool ResultSet::endOfSet() {
    if (ready_raw_rows.empty())
        prepareSomeRows();

    return ready_raw_rows.empty();
}

size_t ResultSet::prepareSomeRows(size_t max_ready_rows) {
    while (!finished && ready_raw_rows.size() < max_ready_rows) {
        if (in.peek() == EOF /* || TODO: reached the end of the current rowset */) {
            finished = true;
            break;
        }

        const auto num_columns = getNumColumns();
        Row row(num_columns);

        for (size_t j = 0; j < num_columns; ++j) {
            readString(in, row.data[j].data, &row.data[j].is_null);
            columns_info[j].display_size
                = std::max<decltype(columns_info[j].display_size)>(row.data[j].data.size(), columns_info[j].display_size);
        }

        ready_raw_rows.emplace_back(std::move(row));
    }

    return ready_raw_rows.size();
}
