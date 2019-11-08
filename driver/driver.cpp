#include "driver/config/ini_defines.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"

#include <chrono>

Driver::Driver() noexcept {
    setAttrSilent(CH_SQL_ATTR_DRIVERLOG, (isYes(INI_DRIVERLOG_DEFAULT) ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF));
    setAttr<std::string>(CH_SQL_ATTR_DRIVERLOGFILE, INI_DRIVERLOGFILE_DEFAULT);
}

Driver::~Driver() {
    // Make sure these are destroyed before anything else.
    environments.clear();
}

Driver & Driver::getInstance() noexcept {
    static Driver driver;
    return driver;
}

const Driver & Driver::getDriver() const noexcept {
    return *this;
}

Driver & Driver::getDriver() noexcept {
    return *this;
}

template <>
Environment & Driver::allocateChild<Environment>() {
    auto child_sptr = std::make_shared<Environment>(*this);
    auto & child = *child_sptr;
    auto handle = child.getHandle();
    environments.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Driver::deallocateChild<Environment>(SQLHANDLE handle) noexcept {
    environments.erase(handle);
}

void Driver::registerDescendant(Object & descendant) {
    descendants.erase(descendant.getHandle());
    descendants.emplace(descendant.getHandle(), std::ref(descendant));
}

void Driver::unregisterDescendant(Object & descendant) noexcept {
    descendants.erase(descendant.getHandle());
}

template <>
Environment * Driver::dynamicCastTo<Environment>(Object * obj) {
    return dynamic_cast<Environment *>(obj);
}

template <>
Connection * Driver::dynamicCastTo<Connection>(Object * obj) {
    return dynamic_cast<Connection *>(obj);
}

template <>
Descriptor * Driver::dynamicCastTo<Descriptor>(Object * obj) {
    return dynamic_cast<Descriptor *>(obj);
}

template <>
Statement * Driver::dynamicCastTo<Statement>(Object * obj) {
    return dynamic_cast<Statement *>(obj);
}

void Driver::onAttrChange(int attr) {
    switch (attr) {
        case CH_SQL_ATTR_DRIVERLOG:
        case CH_SQL_ATTR_DRIVERLOGFILE: {
            bool stream_open = (log_file_stream.is_open() && log_file_stream);
            const bool enable_logging = isLoggingEnabled();
            const auto log_file_attr = getAttrAs<std::string>(CH_SQL_ATTR_DRIVERLOGFILE);

            if (enable_logging) {
                if (stream_open && log_file_attr != log_file_name) {
                    LOG("Switching driver log output to " << (log_file_attr.empty() ? "standard log output" : log_file_attr));
                    writeLogSessionEnd(getLogStream());
                    log_file_stream.close();
                    stream_open = false;
                }

                if (!stream_open) {
                    log_file_name = log_file_attr;
                    log_file_stream = (log_file_name.empty() ? std::ofstream{} : std::ofstream{log_file_name, std::ios_base::out | std::ios_base::app});
                    writeLogSessionStart(getLogStream());
                }
            }
            else {
                if (stream_open) {
                    writeLogSessionEnd(getLogStream());
                    log_file_stream = std::ofstream{};
                }
                log_file_name.clear();
            }

            break;
        }
    }
}

bool Driver::isLoggingEnabled() const {
    return (getAttrAs<SQLUINTEGER>(CH_SQL_ATTR_DRIVERLOG) == SQL_OPT_TRACE_ON);
}

std::ostream & Driver::getLogStream() {
    return (log_file_stream ? log_file_stream : std::clog);
}

void Driver::writeLogMessagePrefix(std::ostream & stream) {
    stream << std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    stream << " [" << getPID() << ":" << getTID() << "]";
}

void Driver::writeLogSessionStart(std::ostream & stream) {
    stream << "==================== ODBC Driver logging session started";
    {
        auto t = std::time(nullptr);
        char mbstr[100] = {};
        if (std::strftime(mbstr, sizeof(mbstr), "%F %T %Z", std::localtime(&t)))
            stream << " (" << mbstr << ")";
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

#if ODBC_UNIXODBC
    stream << " ODBC_UNIXODBC";
#endif

#if defined(UNICODE)
    stream << " UNICODE=" << UNICODE;
    stream << " sizeof(SQLTCHAR)=" << sizeof(SQLTCHAR) << " sizeof(wchar_t)=" << sizeof(wchar_t);
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

void Driver::writeLogSessionEnd(std::ostream & stream) {
    stream << "==================== ODBC Driver logging session ended";
    {
        auto t = std::time(nullptr);
        char mbstr[100] = {};
        if (std::strftime(mbstr, sizeof(mbstr), "%F %T %Z", std::localtime(&t)))
            stream << " (" << mbstr << ")";
    }
    stream << " ====================" << std::endl;
}
