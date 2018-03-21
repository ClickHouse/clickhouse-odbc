#include "diagnostics.h"
#include <Poco/Exception.h>

DiagnosticRecord::DiagnosticRecord()
{
    reset();
}

void DiagnosticRecord::fromException()
{
    try
    {
        throw;
    }
    catch (const SqlException & e)
    {
        message = e.what();
        native_error_code = 1;
        sql_state = e.sqlState();
    }

    catch (const Poco::Exception & e)
    {
        message = e.what() + std::string{": "} + e.message();
        native_error_code = 1;
        sql_state = "HY000";    /// General error.
    }
    catch (const std::exception & e)
    {
        message = e.what();
        native_error_code = 1;
        sql_state = "HY000";    /// General error.
    }
    catch (...)
    {
        message = "Unknown exception.";
        native_error_code = 2;
        sql_state = "HY000";
    }
}

void DiagnosticRecord::reset()
{
    native_error_code = 0;
    sql_state = "-----";
    message.clear();
}