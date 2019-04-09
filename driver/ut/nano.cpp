#include <nanodbc/nanodbc.h>

#include <algorithm>
#include <cstring>
#include <iostream>

//#include "example_unicode_utils.h"
#ifndef NANODBC_UNICODE_UTILS_H
#    define NANODBC_UNICODE_UTILS_H

#    include <nanodbc/nanodbc.h>

#    if defined(__GNUC__) && __GNUC__ < 5
#        include <cwchar>
#    else
#        include <codecvt>
#    endif
#    include <locale>
#    include <string>

#    ifdef NANODBC_USE_IODBC_WIDE_STRINGS
#        error Examples do not support the iODBC wide strings
#    endif

// TODO: These convert utils need to be extracted to a private
//       internal library to share with tests
#    ifdef NANODBC_ENABLE_UNICODE
inline nanodbc::string convert(std::string const & in) {
    static_assert(sizeof(nanodbc::string::value_type) > 1, "NANODBC_ENABLE_UNICODE mode requires wide string");
    nanodbc::string out;
#        if defined(__GNUC__) && __GNUC__ < 5
    std::vector<wchar_t> characters(in.length());
    for (size_t i = 0; i < in.length(); i++)
        characters[i] = in[i];
    const wchar_t * source = characters.data();
    size_t size = wcsnrtombs(nullptr, &source, characters.size(), 0, nullptr);
    if (size == std::string::npos)
        throw std::range_error("UTF-16 -> UTF-8 conversion error");
    out.resize(size);
    wcsnrtombs(&out[0], &source, characters.size(), out.length(), nullptr);
#        elif defined(_MSC_VER) && (_MSC_VER >= 1900)
    // Workaround for confirmed bug in VS2015 and VS2017 too
    // See: https://connect.microsoft.com/VisualStudio/Feedback/Details/1403302
    using wide_char_t = nanodbc::string::value_type;
    auto s = std::wstring_convert<std::codecvt_utf8_utf16<wide_char_t>, wide_char_t>().from_bytes(in);
    auto p = reinterpret_cast<wide_char_t const *>(s.data());
    out.assign(p, p + s.size());
#        else
    out = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(in);
#        endif
    return out;
}

inline std::string convert(nanodbc::string const & in) {
    static_assert(sizeof(nanodbc::string::value_type) > 1, "string must be wide");
    std::string out;
#        if defined(__GNUC__) && __GNUC__ < 5
    size_t size = mbsnrtowcs(nullptr, in.data(), in.length(), 0, nullptr);
    if (size == std::string::npos)
        throw std::range_error("UTF-8 -> UTF-16 conversion error");
    std::vector<wchar_t> characters(size);
    const char * source = in.data();
    mbsnrtowcs(&characters[0], &source, in.length(), characters.size(), nullptr);
    out.resize(size);
    for (size_t i = 0; i < in.length(); i++)
        out[i] = characters[i];
#        elif defined(_MSC_VER) && (_MSC_VER >= 1900)
    // Workaround for confirmed bug in VS2015 and VS2017 too
    // See: https://connect.microsoft.com/VisualStudio/Feedback/Details/1403302
    using wide_char_t = nanodbc::string::value_type;
    std::wstring_convert<std::codecvt_utf8_utf16<wide_char_t>, wide_char_t> convert;
    auto p = reinterpret_cast<const wide_char_t *>(in.data());
    out = convert.to_bytes(p, p + in.size());
#        else
    out = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(in);
#        endif
    return out;
}
#    else
inline nanodbc::string convert(std::string const & in) {
    return in;
}
#    endif

template <typename T>
inline std::string any_to_string(T const & t) {
    return std::to_string(t);
}

template <>
inline std::string any_to_string<nanodbc::string>(nanodbc::string const & t) {
    return convert(t);
}

#endif


// nanodbc/example/usage.cpp

using namespace std;
using namespace nanodbc;

void show(nanodbc::result & results);

void run_test(nanodbc::string const & connection_string) {
    // Establishing connections
    nanodbc::connection connection(connection_string);
    // or connection(connection_string, timeout_seconds);
    // or connection("data source name", "username", "password");
    // or connection("data source name", "username", "password", timeout_seconds);
    cout << "Connected with driver " << convert(connection.driver_name()) << endl;

    {
        {
            auto results = execute(connection, NANODBC_TEXT("select 1+1"));
            show(results);
        }

        execute(connection, NANODBC_TEXT("drop table if exists simple_test;"));
        execute(connection, NANODBC_TEXT("create table simple_test (a int) engine Log;"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (1);"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (2);"));
        {
            auto results = execute(connection, NANODBC_TEXT("select * from simple_test;"));
            show(results);
        }
        execute(connection, NANODBC_TEXT("DROP TABLE IF EXISTS test.strings;"));
        execute(connection, NANODBC_TEXT("CREATE TABLE test.strings (id UInt64, str String, dt DateTime DEFAULT now()) engine = Log;"));
        execute(connection, NANODBC_TEXT("INSERT INTO test.strings SELECT number, hex(number+100000), 1 FROM system.numbers LIMIT 100;"));
        {
            auto results = execute(connection, NANODBC_TEXT("SELECT COUNT(*) FROM test.strings;"));
            show(results);
        }
        {
            auto results = execute(connection, NANODBC_TEXT("SELECT * FROM test.strings LIMIT 100;"));
            show(results);
        }

        {
            auto results = execute(connection,
                NANODBC_TEXT("SELECT `test`.`strings`.`str` AS `platform`, SUM(`test`.`strings`.`id`) AS `sum_installs_ok` FROM "
                             "`test`.`strings` GROUP BY `str`"));
            show(results);
        }

        execute(connection, NANODBC_TEXT("DROP TABLE IF EXISTS test.strings;"));
    }

    // Setup
    execute(connection, NANODBC_TEXT("drop table if exists simple_test;"));
    execute(connection, NANODBC_TEXT("create table simple_test (a int, b varchar) engine Log;"));

    // Direct execution
    {
        execute(connection, NANODBC_TEXT("insert into simple_test values (1, 'one');"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (2, 'two');"));
        execute(connection, NANODBC_TEXT("insert into simple_test values (3, 'tri');"));
        execute(connection, NANODBC_TEXT("insert into simple_test (b) values ('z');"));
        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from simple_test;"));
        show(results);
    }

    // Accessing results by name, or column number
    {
        nanodbc::result results = execute(connection, NANODBC_TEXT("select a as first, b as second from simple_test where a = 1;"));
        results.next();
        auto const value = results.get<nanodbc::string>(1);
        cout << endl << results.get<int>(NANODBC_TEXT("first")) << ", " << convert(value) << endl;
    }

    {
        auto results = execute(connection, NANODBC_TEXT("SELECT (CASE WHEN 1>0 THEN 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' ELSE NULL END);"));
        show(results);
    }

    {
        auto results
            = execute(connection, NANODBC_TEXT("SELECT *, (CASE WHEN 1>0 THEN 'ABC' ELSE 'ABCDEFG' END) FROM system.build_options;"));
        show(results);
    }

    {
        auto results = execute(connection, NANODBC_TEXT("SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'"));
        show(results);
    }

    {
        auto results = execute(connection,
            NANODBC_TEXT("SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN "
                         "'r' ELSE '-' END)  FROM system.numbers LIMIT 5"));
        show(results);
    }

    {
        cout << "Will fail:" << endl;
        try {
            auto results = execute(connection,
                NANODBC_TEXT("SELECT "
                             "-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,"
                             "18446744073709551616,18446744073709551617"));
            show(results);
        } catch (...) {
        }
    }
    {
        auto results = execute(connection,
            NANODBC_TEXT("SELECT "
                         "-127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-"
                         "2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-"
                         "9223372036854775808,9223372036854775806,9223372036854775807"));
        show(results);
    }

    {
        auto results = execute(connection, NANODBC_TEXT("SELECT 2147483647, 2147483648, 2147483647+1, 2147483647+10, 4294967295"));
        show(results);
    }


    // Binding parameters
    if (0) // Not supported. TODO.
    {
        nanodbc::statement statement(connection);

        // Inserting values
        prepare(statement, NANODBC_TEXT("insert into simple_test (a, b) values (?, ?);"));
        const int eight_int = 8;
        statement.bind(0, &eight_int);
        nanodbc::string const eight_str = NANODBC_TEXT("eight");
        statement.bind(1, eight_str.c_str());
        execute(statement);

        // Inserting null values
        prepare(statement, NANODBC_TEXT("insert into simple_test (a, b) values (?, ?);"));
        statement.bind_null(0);
        statement.bind_null(1);
        execute(statement);

        // Inserting multiple null values
        prepare(statement, NANODBC_TEXT("insert into simple_test (a, b) values (?, ?);"));
        statement.bind_null(0, 2);
        statement.bind_null(1, 2);
        execute(statement, 2);

        prepare(statement, NANODBC_TEXT("select * from simple_test;"));
        nanodbc::result results = execute(statement);
        show(results);
    }

    // Transactions
    if (0) // Not supported DELETE. TODO.
    {
        {
            cout << "\ndeleting all rows ... " << flush;
            nanodbc::transaction transaction(connection);
            execute(connection, NANODBC_TEXT("delete from simple_test;"));
            // transaction will be rolled back if we don't call transaction.commit()
        }
        nanodbc::result results = execute(connection, NANODBC_TEXT("select count(1) from simple_test;"));
        results.next();
        cout << "still have " << results.get<int>(0) << " rows!" << endl;
    }

    // Batch inserting
    if (0) // Not supported. TODO.
    {
        nanodbc::statement statement(connection);
        execute(connection, NANODBC_TEXT("drop table if exists batch_test;"));
        execute(connection, NANODBC_TEXT("create table batch_test (x varchar, y int, z float) engine Log;"));
        prepare(statement, NANODBC_TEXT("insert into batch_test (x, x2, y, z) values (?, ?, ?, ?);"));

        const size_t elements = 4;

        nanodbc::string::value_type xdata[elements][10]
            = {NANODBC_TEXT("this"), NANODBC_TEXT("is"), NANODBC_TEXT("a"), NANODBC_TEXT("test")};
        statement.bind_strings(0, xdata);

        std::vector<nanodbc::string> x2data(xdata, xdata + elements);
        statement.bind_strings(1, x2data);

        int ydata[elements] = {1, 2, 3, 4};
        statement.bind(2, ydata, elements);

        float zdata[elements] = {1.1f, 2.2f, 3.3f, 4.4f};
        statement.bind(3, zdata, elements);

        transact(statement, elements);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from batch_test;"), 3);
        show(results);

        execute(connection, NANODBC_TEXT("drop table if exists batch_test;"));
    }

    // Dates and Times
    {
        execute(connection, NANODBC_TEXT("drop table if exists date_test;"));
        execute(connection, NANODBC_TEXT("create table date_test (x datetime) engine Log;"));
        //execute(connection, NANODBC_TEXT("insert into date_test values (current_timestamp);"));
        execute(connection, NANODBC_TEXT("insert into date_test values ({fn current_timestamp});"));

        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from date_test;"));
        results.next();

        nanodbc::date date = results.get<nanodbc::date>(0);
        cout << endl << date.year << "-" << date.month << "-" << date.day << endl;

        results = execute(connection, NANODBC_TEXT("select * from date_test;"));
        show(results);

        execute(connection, NANODBC_TEXT("drop table if exists date_test;"));
    }

    // Inserting NULL values with a sentry
    if (0) // Not supported. TODO.
    {
        nanodbc::statement statement(connection);
        prepare(statement, NANODBC_TEXT("insert into simple_test (a, b) values (?, ?);"));

        const int elements = 5;
        const int a_null = 0;
        nanodbc::string::value_type const * b_null = NANODBC_TEXT("");
        int a_data[elements] = {0, 88, 0, 0, 0};
        nanodbc::string::value_type b_data[elements][10]
            = {NANODBC_TEXT(""), NANODBC_TEXT("non-null"), NANODBC_TEXT(""), NANODBC_TEXT(""), NANODBC_TEXT("")};

        statement.bind(0, a_data, elements, &a_null);
        statement.bind_strings(1, b_data, b_null);

        execute(statement, elements);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from simple_test;"));
        show(results);
    }

    // Inserting NULL values with flags
    if (0) // Not supported. TODO.
    {
        nanodbc::statement statement(connection);
        prepare(statement, NANODBC_TEXT("insert into simple_test (a, b) values (?, ?);"));

        const int elements = 2;
        int a_data[elements] = {0, 42};
        nanodbc::string::value_type b_data[elements][10] = {NANODBC_TEXT(""), NANODBC_TEXT("every")};
        bool nulls[elements] = {true, false};

        statement.bind(0, a_data, elements, nulls);
        statement.bind_strings(1, b_data, nulls);

        execute(statement, elements);

        nanodbc::result results = execute(connection, NANODBC_TEXT("select * from simple_test;"));
        show(results);
    }

    // Cleanup
    execute(connection, NANODBC_TEXT("drop table if exists simple_test;"));
}

void show(nanodbc::result & results) {
    const short columns = results.columns();
    long rows_displayed = 0;

    cout << "\nDisplaying " << results.affected_rows() << " rows " << columns << " columns "
         << "(" << results.rowset_size() << " fetched at a time):" << endl;

    // show the column names
    cout << "row\t";
    for (short i = 0; i < columns; ++i)
        cout << convert(results.column_name(i)) << "\t";
    cout << endl;

    // show the column data for each row
    nanodbc::string const null_value = NANODBC_TEXT("null");
    while (/*!results.at_end() &&*/ results.next()) {
        cout << rows_displayed++ << "\t";
        for (short col = 0; col < columns; ++col) {
            auto const value = results.get<nanodbc::string>(col, null_value);
            cout << "(" << convert(value) << ")\t";
        }
        cout << endl;
    }
}

void usage(ostream & out, std::string const & binary_name) {
    out << "usage: " << binary_name << " connection_string" << endl;
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        char * app_name = strrchr(argv[0], '/');
        app_name = app_name ? app_name + 1 : argv[0];
        if (0 == strncmp(app_name, "lt-", 3))
            app_name += 3; // remove libtool prefix
        usage(cerr, app_name);
        return EXIT_FAILURE;
    }

    try {
        auto const connection_string(convert(argv[1]));
        run_test(connection_string);
        return EXIT_SUCCESS;
    } catch (const exception & e) {
        cerr << e.what() << endl;
    }
    return EXIT_FAILURE;
}
