#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"
#include "driver/attributes.h"
#include "driver/diagnostics.h"
#include "driver/object.h"

#include <Poco/Exception.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

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

    template <typename T> void registerDescendant(T & descendant);
    template <typename T> void unregisterDescendant(T & descendant) noexcept;

    bool isLoggingEnabled() const;
    std::ostream & getLogStream();
    void writeLogMessagePrefix(std::ostream & stream);

    void writeLogSessionStart(std::ostream & stream);
    void writeLogSessionEnd(std::ostream & stream);

protected:
    virtual void onAttrChange(int attr) final override;

private:
    std::string addContextInfoToExceptionMessage(const std::string & message, SQLHANDLE handle, SQLSMALLINT handle_type) const;

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            std::is_invocable_r_v<SQLRETURN, Callable>
        >::type * = nullptr
    ) {
        return callable();
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !std::is_invocable_r_v<SQLRETURN, Callable> &&
            std::is_invocable_v<Callable>
        >::type * = nullptr
    ) {
        callable();
        return SQL_SUCCESS;
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !std::is_invocable_r_v<SQLRETURN, Callable> &&
            !std::is_invocable_v<Callable>
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN doCall(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            std::is_invocable_r_v<SQLRETURN, Callable, ObjectType &>
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
            !std::is_invocable_r_v<SQLRETURN, Callable, ObjectType &> &&
            std::is_invocable_v<Callable, ObjectType &>
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
            !std::is_invocable_r_v<SQLRETURN, Callable, ObjectType &> &&
            !std::is_invocable_v<Callable, ObjectType &>
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

public:
    template <typename Callable>
    inline SQLRETURN call(Callable && callable, SQLHANDLE handle = nullptr, SQLSMALLINT handle_type = 0, bool skip_diag = false) const noexcept;

private:
    std::string log_file_name;
    std::ofstream log_file_stream;

    using DescendantVariantType = std::variant<
        std::reference_wrapper<Statement>,
        std::reference_wrapper<Descriptor>,
        std::reference_wrapper<Connection>,
        std::reference_wrapper<Environment>
    >;

    std::unordered_map<SQLHANDLE, DescendantVariantType> descendants;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Environment>> environments;
};

template <> Environment & Driver::allocateChild<Environment>();
template <> void Driver::deallocateChild<Environment>(SQLHANDLE handle) noexcept;

template <typename T>
void Driver::registerDescendant(T & descendant) {
    descendants.erase(descendant.getHandle());
    descendants.emplace(descendant.getHandle(), std::ref(descendant));
}

template <typename T>
void Driver::unregisterDescendant(T & descendant) noexcept {
    descendants.erase(descendant.getHandle());
}

template <typename Callable>
inline SQLRETURN Driver::call(Callable && callable, SQLHANDLE handle, SQLSMALLINT handle_type, bool skip_diag) const noexcept {
    try {
        if (handle == nullptr) {
            if (handle_type == 0) {
                try {
                    return doCall(callable);
                }
                catch (const SqlException & ex) {
                    LOG(ex.getSQLState() << " (" << ex.what() << ")" << "[rc: " << ex.getReturnCode() << "]");
                    return ex.getReturnCode();
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
        }
        else {
            const auto func = [&] (auto & descendant_ref) noexcept -> SQLRETURN {
                auto & descendant = descendant_ref.get();

                try {
                    return doCall(callable, descendant, skip_diag);
                }
                catch (const SqlException & ex) {
                    auto error_message = addContextInfoToExceptionMessage(ex.what(), handle, handle_type);
                    LOG(ex.getSQLState() << " (" << error_message << ")" << "[rc: " << ex.getReturnCode() << "]");
                    if (!skip_diag)
                        descendant.fillDiag(ex.getReturnCode(), ex.getSQLState(), error_message, 1);
                    return ex.getReturnCode();
                }
                catch (const Poco::Exception & ex) {
                    auto error_message = addContextInfoToExceptionMessage(ex.displayText(), handle, handle_type);
                    LOG("HY000 (" << error_message << ")");
                    if (!skip_diag)
                        descendant.fillDiag(SQL_ERROR, "HY000", error_message, 1);
                    return SQL_ERROR;
                }
                catch (const std::exception & ex) {
                    auto error_message = addContextInfoToExceptionMessage(ex.what(), handle, handle_type);
                    LOG("HY000 (" << error_message << ")");
                    if (!skip_diag)
                        descendant.fillDiag(SQL_ERROR, "HY000", error_message, 1);
                    return SQL_ERROR;
                }
                catch (...) {
                    auto error_message = addContextInfoToExceptionMessage("Unknown exception", handle, handle_type);
                    LOG("HY000 (Unknown exception)");
                    if (!skip_diag)
                        descendant.fillDiag(SQL_ERROR, "HY000", error_message, 2);
                    return SQL_ERROR;
                }
            };

#if defined(WORKAROUND_ALLOW_UNSAFE_DISPATCH)
            // If handle type is provided, and we are not in the "safe dispatch only" mode,
            // we just directly interpret the handle as a pointer to the corresponding class.
            switch (handle_type) {
                case getObjectHandleType<Statement>(): {
                    if constexpr (std::is_invocable_r_v<SQLRETURN, decltype(func), std::reference_wrapper<Statement>>)
                        return func(std::ref(*reinterpret_cast<Statement *>(handle)));
                    break;
                }
                case getObjectHandleType<Descriptor>(): {
                    if constexpr (std::is_invocable_r_v<SQLRETURN, decltype(func), std::reference_wrapper<Descriptor>>)
                        return func(std::ref(*reinterpret_cast<Descriptor *>(handle)));
                    break;
                }
                case getObjectHandleType<Connection>(): {
                    if constexpr (std::is_invocable_r_v<SQLRETURN, decltype(func), std::reference_wrapper<Connection>>)
                        return func(std::ref(*reinterpret_cast<Connection *>(handle)));
                    break;
                }
                case getObjectHandleType<Environment>(): {
                    if constexpr (std::is_invocable_r_v<SQLRETURN, decltype(func), std::reference_wrapper<Environment>>)
                        return func(std::ref(*reinterpret_cast<Environment *>(handle)));
                    break;
                }
            }
#endif

            const auto it = descendants.find(handle);
            if (it != descendants.end()) {
                switch (handle_type) {
                    case 0: {
                        return std::visit(func, it->second);
                    }

                    // Shortcut visitation using std::get_if<>() when 'handle_type' is not 0.
                    // This yields slightly better results with the current compilers optimization capabilities.

                    case getObjectHandleType<Statement>(): {
                        if (auto * descendant_ref_ptr = std::get_if<std::reference_wrapper<Statement>>(&it->second))
                            return func(*descendant_ref_ptr);
                        break;
                    }
                    case getObjectHandleType<Descriptor>(): {
                        if (auto * descendant_ref_ptr = std::get_if<std::reference_wrapper<Descriptor>>(&it->second))
                            return func(*descendant_ref_ptr);
                        break;
                    }
                    case getObjectHandleType<Connection>(): {
                        if (auto * descendant_ref_ptr = std::get_if<std::reference_wrapper<Connection>>(&it->second))
                            return func(*descendant_ref_ptr);
                        break;
                    }
                    case getObjectHandleType<Environment>(): {
                        if (auto * descendant_ref_ptr = std::get_if<std::reference_wrapper<Environment>>(&it->second))
                            return func(*descendant_ref_ptr);
                        break;
                    }
                }
            }
        }
    }
    catch (...) {
        LOG("Unknown exception");
        return SQL_ERROR;
    }

    return SQL_INVALID_HANDLE;
}
