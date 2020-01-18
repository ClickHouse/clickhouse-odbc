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
    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            std::is_invocable_r<SQLRETURN, Callable>::value
        >::type * = nullptr
    ) {
        return callable();
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !std::is_invocable_r<SQLRETURN, Callable>::value &&
            std::is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        callable();
        return SQL_SUCCESS;
    }

    template <typename Callable>
    static inline SQLRETURN doCall(Callable & callable,
        typename std::enable_if<
            !std::is_invocable_r<SQLRETURN, Callable>::value &&
            !std::is_invocable<Callable>::value
        >::type * = nullptr
    ) {
        return SQL_INVALID_HANDLE;
    }

    template <typename Callable, typename ObjectType>
    static inline SQLRETURN doCall(Callable & callable, ObjectType & object, bool skip_diag,
        typename std::enable_if<
            std::is_invocable_r<SQLRETURN, Callable, ObjectType &>::value
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
            !std::is_invocable_r<SQLRETURN, Callable, ObjectType &>::value &&
            std::is_invocable<Callable, ObjectType &>::value
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
            !std::is_invocable_r<SQLRETURN, Callable, ObjectType &>::value &&
            !std::is_invocable<Callable, ObjectType &>::value
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
            const auto it = descendants.find(handle);
            if (it != descendants.end()) {
                return std::visit([&] (auto & descendant_ref) noexcept -> SQLRETURN {
                    auto & descendant = descendant_ref.get();
                    using DescendantType = std::decay_t<decltype(descendant)>;

                    if (
                        handle_type == 0 ||
                        handle_type == getObjectHandleType<DescendantType>()
                    ) {
                        try {
                            return doCall(callable, descendant, skip_diag);
                        }
                        catch (const SqlException & ex) {
                            LOG(ex.getSQLState() << " (" << ex.what() << ")" << "[rc: " << ex.getReturnCode() << "]");
                            if (!skip_diag)
                                descendant.fillDiag(ex.getReturnCode(), ex.getSQLState(), ex.what(), 1);
                            return ex.getReturnCode();
                        }
                        catch (const Poco::Exception & ex) {
                            LOG("HY000 (" << ex.displayText() << ")");
                            if (!skip_diag)
                                descendant.fillDiag(SQL_ERROR, "HY000", ex.displayText(), 1);
                            return SQL_ERROR;
                        }
                        catch (const std::exception & ex) {
                            LOG("HY000 (" << ex.what() << ")");
                            if (!skip_diag)
                                descendant.fillDiag(SQL_ERROR, "HY000", ex.what(), 1);
                            return SQL_ERROR;
                        }
                        catch (...) {
                            LOG("HY000 (Unknown exception)");
                            if (!skip_diag)
                                descendant.fillDiag(SQL_ERROR, "HY000", "Unknown exception", 2);
                            return SQL_ERROR;
                        }
                    }

                    return SQL_INVALID_HANDLE;
                }, it->second);
            }
        }
    }
    catch (...) {
        LOG("Unknown exception");
        return SQL_ERROR;
    }

    return SQL_INVALID_HANDLE;
}
