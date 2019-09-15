#include "descriptor.h"

void DescriptorRecord::on_attr_change(int attr) {
    switch (attr) {
        case SQL_DESC_TYPE: {
            const auto type = get_attr_as<SQLSMALLINT>(SQL_DESC_TYPE);
            if (is_concise_datetime_interval_type(type)) {
                throw SqlException("Inconsistent descriptor information", "HY021");
            }
            else if (is_verbose_type(type)) {
                const auto code = get_attr_as<SQLSMALLINT>(SQL_DESC_DATETIME_INTERVAL_CODE, 0);
                if (code) {
                    set_attr_silent(SQL_DESC_CONCISE_TYPE, convert_datetime_interval_code_to_sql_type(code, type));
                    if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0))
                        set_attr_silent(SQL_DESC_DATA_PTR, 0);
                }
            }
            else {
                set_attr_silent(SQL_DESC_CONCISE_TYPE, type);
                set_attr_silent(SQL_DESC_DATETIME_INTERVAL_CODE, 0);
                if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0))
                    set_attr_silent(SQL_DESC_DATA_PTR, 0);
            }


            // TODO: reset all non-type attributes?


            switch (type) {
                case SQL_CHAR:
                case SQL_VARCHAR:
                    set_attr_silent(SQL_DESC_LENGTH, 1);
                    set_attr_silent(SQL_DESC_PRECISION, 0);
                    break;

                case SQL_DECIMAL:
                case SQL_NUMERIC:
                    set_attr_silent(SQL_DESC_SCALE, 0);
                    set_attr_silent(SQL_DESC_PRECISION, 6); // TODO: fix this.
                    break;

                case SQL_FLOAT:
                    set_attr_silent(SQL_DESC_PRECISION, 6); // TODO: fix this.
                    break;

                case SQL_DATETIME: {
                    const auto code = get_attr_as<SQLSMALLINT>(SQL_DESC_DATETIME_INTERVAL_CODE, 0);
                    switch (code) {
                        case SQL_CODE_DATE:
                        case SQL_CODE_TIME:
                            set_attr_silent(SQL_DESC_PRECISION, 0);
                            break;

                        case SQL_CODE_TIMESTAMP:
                            set_attr_silent(SQL_DESC_PRECISION, 6);
                            break;
                    }
                    break;
                }

                case SQL_INTERVAL: {
                    const auto code = get_attr_as<SQLSMALLINT>(SQL_DESC_DATETIME_INTERVAL_CODE, 0);
                    if (is_interval_code(code)) {
                        set_attr_silent(SQL_DESC_DATETIME_INTERVAL_PRECISION, 2);
                    }
                    if (interval_code_has_second_component(code)) {
                        set_attr_silent(SQL_DESC_PRECISION, 6);
                    }
                    break;
                }
            }

            break;
        }
        case SQL_DESC_CONCISE_TYPE: {
            const auto type = get_attr_as<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE);
            if (is_concise_datetime_interval_type(type)) {
                set_attr_silent(SQL_DESC_DATETIME_INTERVAL_CODE, convert_sql_type_to_datetime_interval_code(type));
                set_attr(SQL_DESC_TYPE, try_convert_sql_type_to_verbose_type(type));
                if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0))
                    set_attr_silent(SQL_DESC_DATA_PTR, 0);
            }
            else if (!is_verbose_type(type)) {
                set_attr_silent(SQL_DESC_DATETIME_INTERVAL_CODE, 0);
                set_attr(SQL_DESC_TYPE, type);
                if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0))
                    set_attr_silent(SQL_DESC_DATA_PTR, 0);
            }
            break;
        }
        case SQL_DESC_DATETIME_INTERVAL_CODE: {
            const auto code = get_attr_as<SQLSMALLINT>(SQL_DESC_DATETIME_INTERVAL_CODE);
            const auto type = get_attr_as<SQLSMALLINT>(SQL_DESC_TYPE, SQL_UNKNOWN_TYPE);
            if (code && is_verbose_type(type)) {
                set_attr_silent(SQL_DESC_CONCISE_TYPE, convert_datetime_interval_code_to_sql_type(code, type));
                if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0))
                    set_attr_silent(SQL_DESC_DATA_PTR, 0);
            }
            break;
        }
        case SQL_DESC_DATA_PTR: {
            if (get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, nullptr))
                consistency_check();
        }
    }
}

bool DescriptorRecord::has_column_size() const {

    // TODO

    return false;
}

SQLULEN DescriptorRecord::get_column_size() const {

    // TODO

    return 0;
}

bool DescriptorRecord::has_decimal_digits() const {

    // TODO

    return false;
}

SQLSMALLINT DescriptorRecord::get_decimal_digits() const {

    // TODO

    return 0;
}

void DescriptorRecord::consistency_check() {

    // TODO

}

Descriptor::Descriptor(Connection & connection)
    : ChildType(connection)
{
    set_attr(SQL_DESC_COUNT, 0);
}

std::size_t Descriptor::get_record_count() const {
    return get_attr_as<SQLSMALLINT>(SQL_DESC_COUNT, 0);
}

DescriptorRecord & Descriptor::get_record(std::size_t num, SQLINTEGER current_role) {
    const auto curr_rec_count = get_attr_as<SQLSMALLINT>(SQL_DESC_COUNT, 0) + 1; // +1 for 0th BOOKMARK record

    for (std::size_t i = curr_rec_count; i <= num && i < records.size(); ++i) {
        get_parent().init_as_desc_rec(records[i], current_role);
    }

    while (records.size() < curr_rec_count || records.size() <= num) {
        records.emplace_back();
        get_parent().init_as_desc_rec(records.back(), current_role);
    }

    if (curr_rec_count <= num) {
        set_attr(SQL_DESC_COUNT, num);
    }

    return records[num];
}
