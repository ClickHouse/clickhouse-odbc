#include "driver.h"
#include "environment.h"
#include "connection.h"
#include "descriptor.h"
#include "statement.h"
#include "win/version.h"

#include <chrono>

Driver::Driver() noexcept {
    set_attr_silent(SQL_ATTR_TRACE, DRIVER_TRACE_DEFAULT);
    set_attr<std::string>(SQL_ATTR_TRACEFILE, DRIVER_TRACEFILE_DEFAULT);
}

Driver::~Driver() {
}

Driver & Driver::get_instance() noexcept {
    static Driver driver;
    return driver;
}

template <>
Environment & Driver::allocate_child<Environment>() {
    auto child_sptr = std::make_shared<Environment>(*this);
    auto & child = *child_sptr;
    auto handle = child.get_handle();
    environments.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Driver::deallocate_child<Environment>(SQLHANDLE handle) noexcept {
    environments.erase(handle);
}

void Driver::register_descendant(Object & descendant) {
    descendants.erase(descendant.get_handle());
    descendants.emplace(descendant.get_handle(), std::ref(descendant));
}

void Driver::unregister_descendant(Object & descendant) noexcept {
    descendants.erase(descendant.get_handle());
}

template <>
Environment * Driver::dynamic_cast_to<Environment>(Object * obj) {
    return dynamic_cast<Environment *>(obj);
}

template <>
Connection * Driver::dynamic_cast_to<Connection>(Object * obj) {
    return dynamic_cast<Connection *>(obj);
}

template <>
Descriptor * Driver::dynamic_cast_to<Descriptor>(Object * obj) {
    return dynamic_cast<Descriptor *>(obj);
}

template <>
Statement * Driver::dynamic_cast_to<Statement>(Object * obj) {
    return dynamic_cast<Statement *>(obj);
}

void Driver::on_attr_change(int attr) {
    switch (attr) {
        case SQL_ATTR_TRACE:
        case SQL_ATTR_TRACEFILE: {
            bool stream_open = (log_file_stream.is_open() && log_file_stream);
            const bool enable_logging = is_logging_enabled();
            const auto tracefile = get_attr_as<std::string>(SQL_ATTR_TRACEFILE);

            if (enable_logging) {
                if (stream_open && tracefile != log_file_name) {
                    LOG("Switching trace output to " << (tracefile.empty() ? "standard log output" : tracefile));
                    write_log_session_end(get_log_stream());
                    log_file_stream.close();
                    stream_open = false;
                }

                if (!stream_open) {
                    log_file_name = tracefile;
                    log_file_stream = (log_file_name.empty() ? std::ofstream{} : std::ofstream{log_file_name, std::ios_base::out | std::ios_base::app});
                    write_log_session_start(get_log_stream());
                }
            }
            else {
                if (stream_open) {
                    write_log_session_end(get_log_stream());
                    log_file_stream = std::ofstream{};
                }
                log_file_name.clear();
            }

            break;
        }
    }
}

bool Driver::is_logging_enabled() const {
    return (get_attr_as<SQLUINTEGER>(SQL_ATTR_TRACE) == SQL_OPT_TRACE_ON);
}

std::ostream & Driver::get_log_stream() {
    return (log_file_stream ? log_file_stream : std::clog);
}

void Driver::write_log_message_prefix(std::ostream & stream) {
    stream << std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    stream << " [PID=" << get_pid() << "]";
    stream << " [THREAD=" << get_tid() << "]";
}

void Driver::write_log_session_start(std::ostream & stream) {
    stream << "==================== ODBC Driver logging session started";
    {
        auto t = std::time(nullptr);
        char mbstr[100] = {};
        if (std::strftime(mbstr, sizeof(mbstr), "%Y.%m.%d %T", std::localtime(&t)))
            stream << " (" << mbstr << ")" << std::endl;
    }
    stream << " ====================" << std::endl;

    stream << " VERSION=" << VERSION_STRING;
#if defined(_win64_)
    stream << " WIN64";
#elif defined(_win32_)
    stream << " WIN32";
#endif
#if ODBC_IODBC
    stream << " ODBC_IODBC";
#endif
#if ODBC_CHAR16
    stream << " ODBC_CHAR16";
#endif
#if ODBC_UNIXODBC
    stream << " ODBC_UNIXODBC";
#endif

#if defined(UNICODE)
    stream << " UNICODE=" << UNICODE;
#    if defined(ODBC_WCHAR)
        stream << " ODBC_WCHAR=" << ODBC_WCHAR;
#    endif
    stream << " sizeof(SQLTCHAR)=" << sizeof(SQLTCHAR) << " sizeof(wchar_t)=" << sizeof(wchar_t);
#endif
#if defined(SQL_WCHART_CONVERT)
    stream << " SQL_WCHART_CONVERT";
#endif
#if ODBCVER
    {
        std::stringstream hstream;
        hstream << " ODBCVER=" << std::hex << ODBCVER << std::dec;
        stream << hstream.str();
    }
#endif
#if defined(ODBC_LIBRARIES)
    stream << " ODBC_LIBRARIES=" << ODBC_LIBRARIES;
#endif
#if defined(ODBC_INCLUDE_DIRECTORIES)
    stream << " ODBC_INCLUDE_DIRECTORIES=" << ODBC_INCLUDE_DIRECTORIES;
#endif
    stream << std::endl;
}

void Driver::write_log_session_end(std::ostream & stream) {
    stream << "==================== ODBC Driver logging session ended";
    {
        auto t = std::time(nullptr);
        char mbstr[100] = {};
        if (std::strftime(mbstr, sizeof(mbstr), "%Y.%m.%d %T", std::localtime(&t)))
            stream << " (" << mbstr << ")" << std::endl;
    }
    stream << " ====================" << std::endl;
}
