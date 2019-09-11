#pragma once

#include "diagnostics.h"
#include "platform.h"
#include "utils.h"

#ifndef NDEBUG
#    if CMAKE_BUILD
#        include "config_cmake.h"
#    endif
#    if USE_DEBUG_17
#        include "iostream_debug_helpers.h"
#    endif
#endif

#include <Poco/Exception.h>

#include <sql.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <unordered_map>
#include <vector>

#include <cstdio>

#define SQL_ATTR_CH_DRIVER_TRACE 1001
#define SQL_ATTR_CH_DRIVER_TRACEFILE 1002

#ifdef NDEBUG
#   define SQL_ATTR_CH_DRIVER_TRACE_DEFAULT SQL_OPT_TRACE_OFF
#else
#   define SQL_ATTR_CH_DRIVER_TRACE_DEFAULT SQL_OPT_TRACE_ON
#endif

#ifdef _win_
#   define SQL_ATTR_CH_DRIVER_TRACEFILE_DEFAULT "\\temp\\clickhouse-odbc.log"
#else
#   define SQL_ATTR_CH_DRIVER_TRACEFILE_DEFAULT "/tmp/clickhouse-odbc.log"
#endif

class AttributeContainer {
public:
    virtual ~AttributeContainer() = default;

    bool has_attr(int attr) const {
        auto iit = attributes_i.find(attr);
        if (iit != attributes_i.end())
            return true;

        auto sit = attributes_s.find(attr);
        if (sit != attributes_s.end())
            return true;

        return false;
    }

    template <typename T>
    T get_attr_as(int attr) const {
        auto it = attributes_i.find(attr);
        return (it == attributes_i.end() ?
            T{} : reinterpret_cast<T>(it->second));
    }

    template <>
    std::string get_attr_as<std::string>(int attr) const {
        auto it = attributes_s.find(attr);
        return (it == attributes_s.end() ?
            std::string{} : it->second);
    }

    template <typename T>
    void set_attr_silent(int attr, T value) {
        attributes_s.erase(attr);
        attributes_i[attr] = reinterpret_cast<std::int64_t>(value);
    }

    void set_attr_string_silent(int attr, const std::string& value) {
        attributes_i.erase(attr);
        attributes_s[attr] = value;
    }

    template <typename T>
    void set_attr(int attr, T value) {
        set_attr_silent(attr, value);
        on_attr_change(attr);
    }

    void set_attr_string(int attr, const std::string& value) {
        set_attr_string_silent(attr, value);
        on_attr_change(attr);
    }

    virtual void on_attr_change(int attr) {
    }

private:
    std::unordered_map<int, std::int64_t> attributes_i;
    std::unordered_map<int, std::string> attributes_s;
};

class DiagnosticsContainer {
public:
    const DiagnosticRecord & get_diag_header() const noexcept;
    void set_diag_header(DiagnosticRecord && rec) noexcept;
    void reset_diag_header() noexcept;

    std::size_t get_diag_rec_count() const noexcept;
    const DiagnosticRecord & get_diag_rec(std::size_t num) const noexcept;
    void insert_diag_rec(DiagnosticRecord && rec) noexcept;
    void reset_diag_recs() noexcept;

private:
    DiagnosticRecord header_record;
    std::vector<DiagnosticRecord> status_records;
};

class Object
    : public AttributeContainer
    , public DiagnosticsContainer
{
public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    Object& operator= (const Object &) = delete;
    Object& operator= (Object &&) = delete;

    explicit Object() noexcept;
    explicit Object(SQLHANDLE h) noexcept;
    virtual ~Object();

    SQLHANDLE get_handle() const noexcept;

private:
    SQLHANDLE const handle;
};

template <typename Parent, typename Self>
class Child
    : public Object
    , public std::enable_shared_from_this<Self>
{
public:
    explicit Child(Parent & p) noexcept
        : parent(p)
    {
    }

    explicit Child(Parent & p, SQLHANDLE h) noexcept
        : Object(h)
        , parent(p)
    {
    }

    Parent & get_parent() const noexcept {
        return parent;
    }

    const Self & get_self() const noexcept {
        return *static_cast<const Self *>(this);
    }

    Self & get_self() noexcept {
        return *static_cast<Self *>(this);
    }

    void deallocate_self() noexcept {
        parent.template deallocate_child<Self>(get_handle());
    }

#if __cplusplus >= 201703L
    std::weak_ptr<Self> weak_from_this() noexcept {
        return this->shared_from_this();
    }

    std::weak_ptr<const Self> weak_from_this() const noexcept {
        return this->shared_from_this();
    }
#endif

    void write_log_prefix(std::ostream & stream) {
        parent.write_log_prefix(stream);
        stream << "[" << get_object_type_name<Self>() << "=" << to_hex_string(get_handle()) << "] ";
    }

private:
    Parent & parent;
};

#define LOG_INTERNAL(file, line, function, context, message) \
    { \
        try { \
            auto & driver_ = Driver::get_instance(); \
            if (driver_.is_logging_enabled()) { \
                auto & stream_ = driver_.get_log_stream(); \
                context.write_log_prefix(stream_); \
                stream_ << file << ":" << line; \
                stream_ << " (in " << function << ")"; \
                stream_ << " " << message << std::endl; \
            } \
        } \
        catch (const std::exception & ex) { \
            std::fprintf(stderr, "Logger exception: %s\n", ex.what()); \
        } \
        catch (...) { \
            std::fprintf(stderr, "Logger exception: unknown\n"); \
        } \
    }

#define LOG_TARGET(context, message) LOG_INTERNAL(__FILE__, __LINE__, __func__, context, message);
#define LOG_LOCAL(message)           LOG_INTERNAL(__FILE__, __LINE__, __func__, *this, message);
#define LOG(message)                 LOG_INTERNAL(__FILE__, __LINE__, __func__, (Driver::get_instance()), message);

#define CALL(callable)                                        Driver::get_instance().call(callable)
#define CALL_WITH_HANDLE(handle, callable)                    Driver::get_instance().call(callable, handle)
#define CALL_WITH_TYPED_HANDLE(handle_type, handle, callable) Driver::get_instance().call(callable, handle, handle_type)

class Environment;
class Connection;
class Descriptor;
class Statement;

class Driver
    : public AttributeContainer
{
private:
    Driver() noexcept;

public:
    Driver(const Driver &) = delete;
    Driver(Driver &&) = delete;
    Driver& operator= (const Driver &) = delete;
    Driver& operator= (Driver &&) = delete;

    ~Driver();

    static Driver & get_instance() noexcept;

    template <typename Callable>
    RETCODE call(Callable && callable, SQLHANDLE handle = nullptr, SQLSMALLINT handle_type = 0) noexcept {
        try {
            Object* obj_ptr = nullptr;

            if (handle == nullptr) {
                if (handle_type != 0)
                    return SQL_INVALID_HANDLE;
            }
            else {
                auto it = descendants.find(handle);
                if (it == descendants.end())
                    return SQL_INVALID_HANDLE;

                obj_ptr = &it->second.get();
            }

            if (obj_ptr == nullptr) {
                try {
                    return do_call(std::forward<Callable>(callable));
                }
                catch (const SqlException & ex) {
                    LOG(ex.displayText());
                    return ex.returnCode();
                }
                catch (const Poco::Exception & ex) {
                    LOG(ex.displayText());
                    return SQL_ERROR;
                }
                catch (const std::exception & ex) {
                    LOG(ex.what());
                    return SQL_ERROR;
                }
                catch (...) {
                    LOG("Unknown exception");
                    return SQL_ERROR;
                }
            }
            else {
                obj_ptr->reset_diag_recs();
                obj_ptr->reset_diag_header();

                try {
#define TRY_DISPATCH_AS(ObjectType) \
                    if (handle_type == 0 || get_object_handle_type<ObjectType>() == handle_type) { \
                        auto * obj = dynamic_cast_to<ObjectType>(obj_ptr); \
                        if (obj) \
                            return do_call(std::forward<Callable>(callable), *obj); \
                    }

                    TRY_DISPATCH_AS(Statement);
                    TRY_DISPATCH_AS(Descriptor);
                    TRY_DISPATCH_AS(Connection);
                    TRY_DISPATCH_AS(Environment);

#undef TRY_DISPATCH_AS
                }
                catch (const SqlException & ex) {
                    LOG(ex.displayText());
                    obj_ptr->set_diag_header(DiagnosticRecord{ex});
                    return ex.returnCode();
                }
                catch (const Poco::Exception & ex) {
                    LOG(ex.displayText());
                    obj_ptr->set_diag_header(DiagnosticRecord{1, "HY000", ex.displayText()});
                    return SQL_ERROR;
                }
                catch (const std::exception & ex) {
                    LOG(ex.what());
                    obj_ptr->set_diag_header(DiagnosticRecord{1, "HY000", ex.what()});
                    return SQL_ERROR;
                }
                catch (...) {
                    LOG("Unknown exception");
                    obj_ptr->set_diag_header(DiagnosticRecord{2, "HY000", "Unknown exception"});
                    return SQL_ERROR;
                }
            }
        }
        catch (...) {
            LOG("Unknown exception");
            return SQL_ERROR;
        }

        return SQL_INVALID_HANDLE;
    }

    // Leave unimplemented for general case.
    template <typename T> T & allocate_child();
    template <typename T> void deallocate_child(SQLHANDLE) noexcept;

    template <> Environment & allocate_child<Environment>();
    template <> void deallocate_child<Environment>(SQLHANDLE handle) noexcept;

    void register_descendant(Object & descendant);
    void unregister_descendant(Object & descendant) noexcept;

    virtual void on_attr_change(int attr) override {
        switch (attr) {
            case SQL_ATTR_CH_DRIVER_TRACE:
            case SQL_ATTR_CH_DRIVER_TRACEFILE: {
                log_file_stream = std::ofstream{};
                if (is_logging_enabled()) {
                    const auto tracefile = get_attr_as<std::string>(SQL_ATTR_CH_DRIVER_TRACEFILE);
                    if (!tracefile.empty()) {
                        log_file_stream = std::ofstream{tracefile, std::ios_base::out | std::ios_base::app};
                    }
                }
                break;
            }
        }
    }

    bool is_logging_enabled() const {
        return (get_attr_as<SQLUINTEGER>(SQL_ATTR_CH_DRIVER_TRACE) == SQL_OPT_TRACE_ON);
    }

    std::ostream & get_log_stream() {
        return (log_file_stream ? log_file_stream : std::clog);
    }

    void write_log_prefix(std::ostream & stream) {
        stream << std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        stream << " [PID=" << get_pid() << "]";
        stream << " [THREAD=" << get_tid() << "]";
    }

private:
    // Leave unimplemented for general case.
    template <typename T> T * dynamic_cast_to(Object *);

    template <> Environment * dynamic_cast_to<Environment>(Object * obj);
    template <> Connection * dynamic_cast_to<Connection>(Object * obj);
    template <> Descriptor * dynamic_cast_to<Descriptor>(Object * obj);
    template <> Statement * dynamic_cast_to<Statement>(Object * obj);

private:
    std::ofstream log_file_stream;

    std::unordered_map<SQLHANDLE, std::reference_wrapper<Object>> descendants;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Environment>> environments;
};
