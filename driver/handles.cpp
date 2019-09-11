#include "driver.h"
#include "environment.h"
#include "connection.h"
#include "descriptor.h"
#include "statement.h"
#include "utils.h"

#include <Poco/Net/HTTPClientSession.h>

namespace {

RETCODE allocEnv(SQLHENV * out_environment_handle) noexcept {
    return Driver::get_instance().call([&] () {
        if (nullptr == out_environment_handle)
            return SQL_INVALID_HANDLE;

        *out_environment_handle = Driver::get_instance().allocate_child<Environment>().get_handle();
        return SQL_SUCCESS;
    });
}

RETCODE allocConnect(SQLHENV environment_handle, SQLHDBC * out_connection_handle) noexcept {
    return Driver::get_instance().callWith(environment_handle, [&] (Environment & environment) {
        if (nullptr == out_connection_handle)
            return SQL_INVALID_HANDLE;

        *out_connection_handle = environment.allocate_child<Connection>().get_handle();
        return SQL_SUCCESS;
    });
}

RETCODE allocStmt(SQLHDBC connection_handle, SQLHSTMT * out_statement_handle) noexcept {
    return Driver::get_instance().callWith(connection_handle, [&] (Connection & connection) {
        if (nullptr == out_statement_handle)
            return SQL_INVALID_HANDLE;

        *out_statement_handle = connection.allocate_child<Statement>().get_handle();
        return SQL_SUCCESS;
    });
}

RETCODE allocDesc(SQLHDBC connection_handle, SQLHDESC * out_descriptor_handle) noexcept {
    return Driver::get_instance().callWith(connection_handle, [&] (Connection & connection) {
        if (nullptr == out_descriptor_handle)
            return SQL_INVALID_HANDLE;

        *out_descriptor_handle = connection.allocate_child<Descriptor>().get_handle();
        return SQL_SUCCESS;
    });
}

RETCODE freeHandle(SQLHANDLE handle) noexcept {
    return Driver::get_instance().callWith(handle, [&] (auto & object) {
        object.deallocate_self();
        return SQL_SUCCESS;
    });
}

} // namespace


extern "C" {

RETCODE SQL_API SQLAllocHandle(SQLSMALLINT handle_type, SQLHANDLE input_handle, SQLHANDLE * output_handle) {
    LOG(__FUNCTION__ << " handle_type=" << handle_type << " input_handle=" << input_handle);

    switch (handle_type) {
        case SQL_HANDLE_ENV:
            return allocEnv((SQLHENV *)output_handle);
        case SQL_HANDLE_DBC:
            return allocConnect((SQLHENV)input_handle, (SQLHDBC *)output_handle);
        case SQL_HANDLE_STMT:
            return allocStmt((SQLHDBC)input_handle, (SQLHSTMT *)output_handle);
        case SQL_HANDLE_DESC:
            return allocDesc((SQLHDBC)input_handle, (SQLHDESC *)output_handle);
        default:
            LOG("AllocHandle: Unknown handleType=" << handleType);
            return SQL_ERROR;
    }
}

RETCODE SQL_API SQLAllocEnv(SQLHDBC * output_handle) {
    LOG(__FUNCTION__);
    return allocEnv(output_handle);
}

RETCODE SQL_API SQLAllocConnect(SQLHENV input_handle, SQLHDBC * output_handle) {
    LOG(__FUNCTION__ << " input_handle=" << input_handle);
    return allocConnect(input_handle, output_handle);
}

RETCODE SQL_API SQLAllocStmt(SQLHDBC input_handle, SQLHSTMT * output_handle) {
    LOG(__FUNCTION__ << " input_handle=" << input_handle);
    return allocStmt(input_handle, output_handle);
}

RETCODE SQL_API SQLFreeHandle(SQLSMALLINT handleType, SQLHANDLE handle) {
    LOG(__FUNCTION__ << " handleType=" << handleType << " handle=" << handle);

    switch (handleType) {
        case SQL_HANDLE_ENV:
        case SQL_HANDLE_DBC:
        case SQL_HANDLE_STMT:
        case SQL_HANDLE_DESC:
            return freeHandle(handle);
        default:
            LOG("FreeHandle: Unknown handleType=" << handleType);
            return SQL_ERROR;
    }
}

RETCODE SQL_API SQLFreeEnv(HENV handle) {
    LOG(__FUNCTION__);
    return freeHandle(handle);
}

RETCODE SQL_API SQLFreeConnect(HDBC handle) {
    LOG(__FUNCTION__);
    return freeHandle(handle);
}

RETCODE SQL_API SQLFreeStmt(HSTMT statement_handle, SQLUSMALLINT option) {
    LOG(__FUNCTION__ << " option=" << option);

    return Driver::get_instance().callWith(statement_handle, [&] (Statement & statement) {
        switch (option) {
            case SQL_CLOSE: /// Close the cursor, ignore the remaining results. If there is no cursor, then noop.
                statement.reset();



                //-/




            case SQL_DROP:
                return freeHandle(handle);

            case SQL_UNBIND:
                statement.bindings.clear();
                return SQL_SUCCESS;

            case SQL_RESET_PARAMS:



                //-/



                return SQL_ERROR;

            default:
                return SQL_ERROR;
        }
    });
}

} // extern "C"
