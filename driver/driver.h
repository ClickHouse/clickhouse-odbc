#pragma once

#include "platform.h"
#include "utils.h"
#include "attributes.h"
#include "diagnostics.h"
#include "object.h"

#include <Poco/Exception.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <cstddef>
#include <cstdio>

#define LOG_INTERNAL(file, line, function, context, message) \
    { \
        try { \
            auto & context_ = context; \
            if (context_.is_logging_enabled()) { \
                auto & stream_ = context_.get_log_stream(); \
                context_.write_log_message_prefix(stream_); \
                stream_ << " " << file << ":" << line; \
                stream_ << " in " << function << ": "; \
                stream_ << message << std::endl; \
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
#define LOG_LOCAL(message)           LOG_INTERNAL(__FILE__, __LINE__, __func__, (*this), message);
#define LOG(message)                 LOG_INTERNAL(__FILE__, __LINE__, __func__, (Driver::get_instance()), message);

#define CALL(callable)                                                  (Driver::get_instance().call(callable))
#define CALL_WITH_HANDLE(handle, callable)                              (Driver::get_instance().call(callable, handle))
#define CALL_WITH_HANDLE_SKIP_DIAG(handle, callable)                    (Driver::get_instance().call(callable, handle, 0, true))
#define CALL_WITH_TYPED_HANDLE(handle_type, handle, callable)           (Driver::get_instance().call(callable, handle, handle_type))
#define CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, callable) (Driver::get_instance().call(callable, handle, handle_type, true))

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

    // Leave unimplemented for general case.
    template <typename T> T & allocate_child();
    template <typename T> void deallocate_child(SQLHANDLE) noexcept;

    void register_descendant(Object & descendant);
    void unregister_descendant(Object & descendant) noexcept;

    virtual void on_attr_change(int attr) override;

    bool is_logging_enabled() const;
    std::ostream & get_log_stream();
    void write_log_message_prefix(std::ostream & stream);

    void write_log_session_start(std::ostream & stream);
    void write_log_session_end(std::ostream & stream);

private:
    // Leave unimplemented for general case.
    template <typename T> T * dynamic_cast_to(Object *);

    template <typename Callable>
    static inline SQLRETURN do_call(Callable & callable,
        typename std::enable_if<
            is_invocable_r<SQLRETURN, Callable>::value
        >::type * = nullptr
    ) {
        return callable();
    }

    template <typename Callable>
    static inline SQLRETURN do_call(Callable & callable,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable>::value &&
            is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        callable();
        return SQL_SUCCESS;
    }

    template <typename Callable>
    static inline SQLRETURN do_call(Callable & callable,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable>::value &&
            !is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN do_call(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            is_invocable_r<SQLRETURN, Callable, ObjectType &>::value
        >::type * = nullptr
    ) {
        SQLRETURN rc = SQL_SUCCESS;
        if (!skip_diag) {
            object.reset_statuses();
            object.reset_header();
        }
        rc = callable(object);
        if (!skip_diag) {
            object.set_return_code(rc);
        }
        return rc;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN do_call(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable, ObjectType &>::value &&
            is_invocable<Callable, ObjectType &>::value
        >::type * = nullptr
    ) {
        SQLRETURN rc = SQL_SUCCESS;
        if (!skip_diag) {
            object.reset_statuses();
            object.reset_header();
        }
        callable(object);
        if (!skip_diag) {
            rc = object.get_return_code();
        }
        return rc;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN do_call(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable, ObjectType &>::value &&
            !is_invocable<Callable, ObjectType &>::value
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

public:
    template <typename Callable>
    SQLRETURN call(Callable && callable, SQLHANDLE handle = nullptr, SQLSMALLINT handle_type = 0, bool skip_diag = false) noexcept;

private:
    std::string log_file_name;
    std::ofstream log_file_stream;

    // TODO: consider upgrading from common Object type to std::variant of C++17 (or Boost), when available.
    std::unordered_map<SQLHANDLE, std::reference_wrapper<Object>> descendants;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Environment>> environments;
};

template <> Environment & Driver::allocate_child<Environment>();
template <> void Driver::deallocate_child<Environment>(SQLHANDLE handle) noexcept;

// Move this to cpp to avoid the need of fully specifying these types here.
template <> Environment * Driver::dynamic_cast_to<Environment>(Object * obj);
template <> Connection * Driver::dynamic_cast_to<Connection>(Object * obj);
template <> Descriptor * Driver::dynamic_cast_to<Descriptor>(Object * obj);
template <> Statement * Driver::dynamic_cast_to<Statement>(Object * obj);

template <typename Callable>
SQLRETURN Driver::call(Callable && callable, SQLHANDLE handle, SQLSMALLINT handle_type, bool skip_diag) noexcept {
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
                return do_call(callable);
            }
            catch (const SqlException & ex) {
                LOG(ex.get_sql_state() << " (" << ex.what() << ")");
                return SQL_ERROR;
            }
            catch (const Poco::Exception & ex) {
                LOG("HY000 (" << ex.displayText() << ")");
                return SQL_ERROR;
            }
            catch (const std::exception & ex) {
                LOG("HY000 (" << ex.what() << ")");
                return SQL_ERROR;
            }
            catch (...) {
                LOG("HY000 (Unknown exception)");
                return SQL_ERROR;
            }
        }
        else {
            try {

#define TRY_DISPATCH_AS(ObjectType) \
    if ( \
        handle_type == 0 || \
        handle_type == get_object_handle_type<ObjectType>() \
    ) { \
        auto * obj = dynamic_cast_to<ObjectType>(obj_ptr); \
        if (obj) \
            return do_call(callable, *obj, skip_diag); \
    }

                TRY_DISPATCH_AS(Statement);
                TRY_DISPATCH_AS(Descriptor);
                TRY_DISPATCH_AS(Connection);
                TRY_DISPATCH_AS(Environment);

#undef TRY_DISPATCH_AS

            }
            catch (const SqlException & ex) {
                LOG(ex.get_sql_state() << " (" << ex.what() << ")");
                if (!skip_diag)
                    obj_ptr->fill(SQL_ERROR, ex.get_sql_state(), ex.what(), 1);
                return SQL_ERROR;
            }
            catch (const Poco::Exception & ex) {
                LOG("HY000 (" << ex.displayText() << ")");
                if (!skip_diag)
                    obj_ptr->fill(SQL_ERROR, "HY000", ex.displayText(), 1);
                return SQL_ERROR;
            }
            catch (const std::exception & ex) {
                LOG("HY000 (" << ex.what() << ")");
                if (!skip_diag)
                    obj_ptr->fill(SQL_ERROR, "HY000", ex.what(), 1);
                return SQL_ERROR;
            }
            catch (...) {
                LOG("HY000 (Unknown exception)");
                if (!skip_diag)
                    obj_ptr->fill(SQL_ERROR, "HY000", "Unknown exception", 2);
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
