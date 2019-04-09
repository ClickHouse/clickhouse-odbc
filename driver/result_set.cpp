#include "result_set.h"

#include "log/log.h"
#include "statement.h"

namespace {

class EmptyMutator : public IResultMutator {
public:
    void UpdateColumnInfo(std::vector<ColumnInfo> *) override {}

    void UpdateRow(const std::vector<ColumnInfo> &, Row *) override {}
};

} // namespace

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

void ResultSet::init(Statement * statement_, IResultMutatorPtr mutator_) {
    statement = statement_;
    mutator = (mutator_ ? std::move(mutator_) : IResultMutatorPtr(new EmptyMutator));

    if (in().peek() == EOF)
        return;

    int32_t num_header_rows = 0;
    readSize(in(), num_header_rows);
    if (!num_header_rows)
        return;

    for (size_t row_n = 0; row_n < num_header_rows; ++row_n) {
        /// Title: number of columns, their names and types.
        int32_t num_columns = 0;
        readSize(in(), num_columns);

        if (num_columns <= 1)
            return;

        std::string row_name;
        readString(in(), row_name);
        --num_columns;

        if (row_name == "name") {
            columns_info.resize(num_columns);
            for (size_t i = 0; i < num_columns; ++i) {
                readString(in(), columns_info[i].name);
            }
        } else if (row_name == "type") {
            columns_info.resize(num_columns);
            for (size_t i = 0; i < num_columns; ++i) {
                readString(in(), columns_info[i].type);
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
                readString(in(), dummy);
            }
        }
    }
    mutator->UpdateColumnInfo(&columns_info);

    readNextBlock();
}

bool ResultSet::empty() const {
    return columns_info.empty();
}

size_t ResultSet::getNumColumns() const {
    return columns_info.size();
}

const ColumnInfo & ResultSet::getColumnInfo(size_t i) const {
    return columns_info.at(i);
}

size_t ResultSet::getNumRows() const {
    return rows;
}

Row ResultSet::fetch() {
    if (empty())
        return {};

    if (current_block.data.end() == iterator && !readNextBlock())
        return {};

    ++rows;
    if (statement->rows_fetched_ptr)
        *statement->rows_fetched_ptr = rows;

    const Row & row = *iterator;
    ++iterator;
    return row;
}

std::istream & ResultSet::in() {
    return *statement->in;
}

bool ResultSet::readNextBlockCache() {
    size_t max_block_size = 1000; // How many rows read to calculate max columns sizes
    size_t readed = 0;
    for (size_t i = 0; i < max_block_size && in().peek() != EOF; ++i) {
        size_t num_columns = getNumColumns();
        Row row(num_columns);

        for (size_t j = 0; j < num_columns; ++j) {
            readString(in(), row.data[j].data, &row.data[j].is_null);
            columns_info[j].display_size
                = std::max<decltype(columns_info[j].display_size)>(row.data[j].data.size(), columns_info[j].display_size);

            //LOG("read Row/Col " << i <<":"<< j << " name=" << row.data[j].data << " display_size=" << columns_info[j].display_size);
        }

        current_block_buffer.emplace_back(std::move(row));
        ++readed;
    }

    return readed;
}

bool ResultSet::readNextBlock() {
    auto max_block_size = statement->row_array_size;

    current_block.data.clear();
    current_block.data.reserve(max_block_size);

    for (size_t i = 0; i < max_block_size && (current_block_buffer.size() || readNextBlockCache()); ++i) {
        auto row = current_block_buffer.front();
        current_block_buffer.pop_front();

        mutator->UpdateRow(columns_info, &row);

        current_block.data.emplace_back(std::move(row));
    }

    iterator = current_block.data.begin();
    return !current_block.data.empty();
}

void ResultSet::throwIncompleteResult() const {
    throw std::runtime_error("Incomplete result received.");
}
