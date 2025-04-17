#pragma once

#include "driver/result_set.h"
#include "driver/statement.h"
#include <Poco/Timezone.h>

/**
 * This mutator injects missing information in `systems.columns` required by SQLColumns,
 * such as SQL type (e.g., SQL_TINYINT, SQL_INTEGER), column value, octet length, and other
 * details stored only in the driver.
 *
 * The result set must match the structure described at:
 * https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function
 */
class SQLColumnsResultSetMutator
    : public ResultMutator
{
public:
    explicit SQLColumnsResultSetMutator(Statement & statement_)
        : statement{statement_}
    {
    }

    void transformRow(const std::vector<ColumnInfo> & /*unused*/, Row & row) override;


private:

    Statement & statement;

};
