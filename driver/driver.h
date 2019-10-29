#pragma once

#include "platform/platform.h"
#include "utils/utils.h"
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
            if (context_.isLoggingEnabled()) { \
                auto & stream_ = context_.getLogStream(); \
                context_.writeLogMessagePrefix(stream_); \
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
#define LOG(message)                 LOG_INTERNAL(__FILE__, __LINE__, __func__, (Driver::getInstance()), message);

#define CALL(callable)                                                  (Driver::getInstance().call(callable))
#define CALL_WITH_HANDLE(handle, callable)                              (Driver::getInstance().call(callable, handle))
#define CALL_WITH_HANDLE_SKIP_DIAG(handle, callable)                    (Driver::getInstance().call(callable, handle, 0, true))
#define CALL_WITH_TYPED_HANDLE(handle_type, handle, callable)           (Driver::getInstance().call(callable, handle, handle_type))
#define CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, callable) (Driver::getInstance().call(callable, handle, handle_type, true))

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

    static Driver & getInstance() noexcept;

    const Driver & getDriver() const noexcept;
    Driver & getDriver() noexcept;

    // Leave unimplemented for general case.
    template <typename T> T & allocateChild();
    template <typename T> void deallocateChild(SQLHANDLE) noexcept;

    void registerDescendant(Object & descendant);
    void unregisterDescendant(Object & descendant) noexcept;

    bool isLoggingEnabled() const;
    std::ostream & getLogStream();
    void writeLogMessagePrefix(std::ostream & stream);

    void writeLogSessionStart(std::ostream & stream);
    void writeLogSessionEnd(std::ostream & stream);

protected:
    virtual void onAttrChange(int attr) final override;

private:
    // Leave unimplemented for general case.
    template <typename T> T * dynamicCastTo(Object *);

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            is_invocable_r<SQLRETURN, Callable>::value
        >::type * = nullptr
    ) {
        return callable();
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable>::value &&
            is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        callable();
        return SQL_SUCCESS;
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable>::value &&
            !is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN doCall(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            is_invocable_r<SQLRETURN, Callable, ObjectType &>::value
        >::type * = nullptr
    ) {
        SQLRETURN rc = SQL_SUCCESS;
        if (!skip_diag) {
            object.resetDiag();
        }
        rc = callable(object);
        if (!skip_diag) {
            object.setReturnCode(rc);
        }
        return rc;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN doCall(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            !is_invocable_r<SQLRETURN, Callable, ObjectType &>::value &&
            is_invocable<Callable, ObjectType &>::value
        >::type * = nullptr
    ) {
        SQLRETURN rc = SQL_SUCCESS;
        if (!skip_diag) {
            object.resetDiag();
        }
        callable(object);
        if (!skip_diag) {
            rc = object.getReturnCode();
        }
        return rc;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN doCall(Callable & callable, ObjectType & object, bool skip_diag,
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

template <> Environment & Driver::allocateChild<Environment>();
template <> void Driver::deallocateChild<Environment>(SQLHANDLE handle) noexcept;

// Move this to cpp to avoid the need of fully specifying these types here.
template <> Environment * Driver::dynamicCastTo<Environment>(Object * obj);
template <> Connection * Driver::dynamicCastTo<Connection>(Object * obj);
template <> Descriptor * Driver::dynamicCastTo<Descriptor>(Object * obj);
template <> Statement * Driver::dynamicCastTo<Statement>(Object * obj);

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
                return doCall(callable);
            }
            catch (const SqlException & ex) {
                LOG(ex.getSQLState() << " (" << ex.what() << ")");
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
        handle_type == getObjectHandleType<ObjectType>() \
    ) { \
        auto * obj = dynamicCastTo<ObjectType>(obj_ptr); \
        if (obj) \
            return doCall(callable, *obj, skip_diag); \
    }

                TRY_DISPATCH_AS(Statement);
                TRY_DISPATCH_AS(Descriptor);
                TRY_DISPATCH_AS(Connection);
                TRY_DISPATCH_AS(Environment);

#undef TRY_DISPATCH_AS

            }
            catch (const SqlException & ex) {
                LOG(ex.getSQLState() << " (" << ex.what() << ")");
                if (!skip_diag)
                    obj_ptr->fillDiag(SQL_ERROR, ex.getSQLState(), ex.what(), 1);
                return SQL_ERROR;
            }
            catch (const Poco::Exception & ex) {
                LOG("HY000 (" << ex.displayText() << ")");
                if (!skip_diag)
                    obj_ptr->fillDiag(SQL_ERROR, "HY000", ex.displayText(), 1);
                return SQL_ERROR;
            }
            catch (const std::exception & ex) {
                LOG("HY000 (" << ex.what() << ")");
                if (!skip_diag)
                    obj_ptr->fillDiag(SQL_ERROR, "HY000", ex.what(), 1);
                return SQL_ERROR;
            }
            catch (...) {
                LOG("HY000 (Unknown exception)");
                if (!skip_diag)
                    obj_ptr->fillDiag(SQL_ERROR, "HY000", "Unknown exception", 2);
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
