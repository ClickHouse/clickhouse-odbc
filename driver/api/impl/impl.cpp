#include "driver/api/impl/impl.h"
#include "driver/utils/utils.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"

#include <Poco/Net/HTTPClientSession.h>

#include <type_traits>

namespace impl {

SQLRETURN allocEnv(SQLHENV * out_environment_handle) noexcept {
    return CALL([&] () {
        if (nullptr == out_environment_handle)
            return SQL_INVALID_HANDLE;

        *out_environment_handle = Driver::getInstance().allocateChild<Environment>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocConnect(SQLHENV environment_handle, SQLHDBC * out_connection_handle) noexcept {
    return CALL_WITH_HANDLE(environment_handle, [&] (Environment & environment) {
        if (nullptr == out_connection_handle)
            return SQL_INVALID_HANDLE;

        *out_connection_handle = environment.allocateChild<Connection>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocStmt(SQLHDBC connection_handle, SQLHSTMT * out_statement_handle) noexcept {
    return CALL_WITH_HANDLE(connection_handle, [&] (Connection & connection) {
        if (nullptr == out_statement_handle)
            return SQL_INVALID_HANDLE;

        *out_statement_handle = connection.allocateChild<Statement>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocDesc(SQLHDBC connection_handle, SQLHDESC * out_descriptor_handle) noexcept {
    return CALL_WITH_HANDLE(connection_handle, [&] (Connection & connection) {
        if (nullptr == out_descriptor_handle)
            return SQL_INVALID_HANDLE;

        auto & descriptor = connection.allocateChild<Descriptor>();
        connection.initAsAD(descriptor, true);
        *out_descriptor_handle = descriptor.getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN freeHandle(SQLHANDLE handle) noexcept {
    return CALL_WITH_HANDLE_SKIP_DIAG(handle, [&] (auto & object) {
        if ( // Refuse to manually deallocate an automatically allocated descriptor.
            std::is_convertible<std::decay<decltype(object)> *, Descriptor *>::value &&
            object.template getAttrAs<SQLSMALLINT>(SQL_DESC_ALLOC_TYPE) != SQL_DESC_ALLOC_USER
        ) {
            return SQL_ERROR;
        }

        object.deallocateSelf();
        return SQL_SUCCESS;
    });
}

} // namespace impl
