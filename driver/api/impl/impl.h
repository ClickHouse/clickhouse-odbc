#include "driver/platform/platform.h"

namespace impl {

    SQLRETURN allocEnv(SQLHENV * out_environment_handle) noexcept;
    SQLRETURN allocConnect(SQLHENV environment_handle, SQLHDBC * out_connection_handle) noexcept;
    SQLRETURN allocStmt(SQLHDBC connection_handle, SQLHSTMT * out_statement_handle) noexcept;
    SQLRETURN allocDesc(SQLHDBC connection_handle, SQLHDESC * out_descriptor_handle) noexcept;
    SQLRETURN freeHandle(SQLHANDLE handle) noexcept;

} // namespace impl
