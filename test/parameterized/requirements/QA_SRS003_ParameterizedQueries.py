# These are auto generated requirements from an SRS document.
# Do not edit by hand but re-generate instead
# using "tfs requirement generate" command.
#
from testflows.core import Requirement

RQ_SRS_003_ParameterizedQueries = Requirement(
        name='RQ.SRS-003.ParameterizedQueries',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support parameterized queries as described in [SQL Statement Parameters].\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataTypes = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataTypes',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'The ODBC driver SHALL support using parameters for all applicable [ClickHouse] data types.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_pyodbc = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.pyodbc',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support executing parameterized queries using [pyodbc] connector.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_unixODBC_isql = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.unixODBC.isql',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support executing parameterized queries using [isql] connector\n'
        'from [unixODBC] project.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_unixODBC_iusql = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.unixODBC.iusql',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support executing parameterized queries using [iusql] connector\n'
        'from [unixODBC] project.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_iODBC_iodbctest = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.iODBC.iodbctest',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support executing parameterized queries using [iodbctest] connector\n'
        'from [iODBC] project.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_iODBC_iodbctestw = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.iODBC.iodbctestw',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support executing parameterized queries using [iodbctestw] connector\n'
        'from [iODBC] project.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_Syntax_Select_Parameters = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.Syntax.Select.Parameters',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support parameters in the `SELECT` statement using the syntax\n'
        'SELECT PartID, Description, Price FROM Parts WHERE PartID = ? AND Description = ? AND Price = ? \n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Int8 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Int8',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns \n'
        'with `Int8` data type having ranges `[-128 : 127]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Int16 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Int16',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns \n'
        'with `Int16` data type having ranges `[-32768 : 32767]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Int32 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Int32',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Int32` data type having ranges `[-2147483648 : 2147483647]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Int64 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Int64',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Int64` data type having ranges `[-9223372036854775808 : 9223372036854775807]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt8 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt8',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `UInt8` data type having ranges `[0 : 255]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt16 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt16',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `UInt16` data type having ranges `[0 : 65535]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt32 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt32',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `UInt32` data type having ranges `[0 : 4294967295]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt64 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt64',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `UInt64` data type having ranges `[0 : 18446744073709551615]`.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Float32` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32_Inf = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.Inf',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns \n'
        'with `Float32` data type having value  \n'
        '`Inf` (positive infinity) and `-Inf` (negative infinity).\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32_NaN = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.NaN',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns \n'
        'with `Float32` data type having value `Nan` (not a number).\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Float64` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64_Inf = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.Inf',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Float64` data type having value \n'
        '`Inf` (positive infinity) and `-Inf` (negative infinity).\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64_NaN = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.NaN',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Float64` data type having value `NaN` (not a number).\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal32 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal32',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Decimal32` data type having ranges\n'
        '`[-(1 * 10^(9 - S)-(1 / (10^S))) : 1 * 10^(9 - S) - (1 / (10^S)]`\n'
        '* **P** precision. Valid range: [ 1 : 38 ]. \n'
        'Determines how many decimal digits number can have (including fraction).\n'
        '* **S** scale. Valid range: [ 0 : P ]. \n'
        'Determines how many decimal digits fraction can have.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal64 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal64',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns \n'
        'with `Decimal64` data type having ranges\n'
        '`[-(1 * 10^(18 - S)-(1 / (10^S))) : 1 * 10^(18 - S) - (1 / (10^S)]`\n'
        '* **P** precision. Valid range: [ 1 : 38 ]. \n'
        'Determines how many decimal digits number can have (including fraction).\n'
        '* **S** scale. Valid range: [ 0 : P ]. \n'
        'Determines how many decimal digits fraction can have.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal128 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal128',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Decimal128` data type having ranges\n'
        '`[-(1 * 10^(38 - S)-(1 / (10^S))) : 1 * 10^(38 - S) - (1 / (10^S)]`\n'
        '* **P** precision. Valid range: [ 1 : 38 ]. \n'
        'Determines how many decimal digits number can have (including fraction).\n'
        '* **S** scale. Valid range: [ 0 : P ]. \n'
        'Determines how many decimal digits fraction can have.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String_ASCII = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String.ASCII',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type containing ASCII encoded strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String_UTF8 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String.UTF8',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type containing UTF-8 encoded strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String_Unicode = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Unicode',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type containing Unicode encoded strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String_Binary = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Binary',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type containing binary data.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_String_Empty = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Empty',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `String` data type containing empty value.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_FixedString = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.FixedString',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `FixedString` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Date = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Date',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Date` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_DateTime = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.DateTime',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `DateTime` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Enum = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Enum',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Enum` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_UUID = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.UUID',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `UUID` data type and treat them like strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_IPv6 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv6',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `IPv6` data type and treat them like strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_IPv4 = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv4',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `IPv4` data type and treat them like strings.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Nullable = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by the columns\n'
        'with `Nullable` data type.\n'
        ),
        link=None
    )

RQ_SRS_003_ParameterizedQueries_DataType_Select_Nullable_NULL = Requirement(
        name='RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable.NULL',
        version='1.0',
        priority=None,
        group=None,
        type=None,
        uid=None,
        description=(
        'ODBC driver SHALL support using parameters for selecting columns and filtering by columns\n'
        'with `Nullable` data type containing `NULL` value.\n'
        ),
        link=None
    )
