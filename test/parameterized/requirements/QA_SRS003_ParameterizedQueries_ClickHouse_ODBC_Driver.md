# QA-SRS003-ParameterizedQueries ClickHouse ODBC Driver Parameterized Queries - Software Requirements Specification

(c) 2019 Altinity LTD. All Rights Reserved.

**Document status:** Confidential

**Author:** vzakaznikov@altinity.com

**Date:** September 23, 2019

# Approval

**Status:** -

**Version:** -

**Approved by:** -

**Date:** -

# Table of Contents
* [Revision History](#revision-history)
* 1 [Introduction](#introduction)
* 2 [Parent Specification](#parent-specification)
* 3 [Requirements](#requirements)
  * 3.1 [General](#general)
    * 3.1.1 [RQ.SRS-003.ParameterizedQueries](#rqsrs-003parameterizedqueries)
    * 3.1.2 [RQ.SRS-003.ParameterizedQueries.DataTypes](#rqsrs-003parameterizedqueriesdatatypes)
    * 3.1.3 [RQ.SRS-003.ParameterizedQueries.pyodbc](#rqsrs-003parameterizedqueriespyodbc)
    * 3.1.4 [RQ.SRS-003.ParameterizedQueries.unixODBC.isql](#rqsrs-003parameterizedqueriesunixodbcisql)
    * 3.1.5 [RQ.SRS-003.ParameterizedQueries.unixODBC.iusql](#rqsrs-003parameterizedqueriesunixodbciusql)
    * 3.1.6 [RQ.SRS-003.ParameterizedQueries.iODBC.iodbctest](#rqsrs-003parameterizedqueriesiodbciodbctest)
    * 3.1.7 [RQ.SRS-003.ParameterizedQueries.iODBC.iodbctestw](#rqsrs-003parameterizedqueriesiodbciodbctestw)
  * 3.2 [Specific](#specific)
    * 3.2.1 [Syntax](#syntax)
      * 3.2.1.1 [RQ.SRS-003.ParameterizedQueries.Syntax.Select.Parameters](#rqsrs-003parameterizedqueriessyntaxselectparameters)
    * 3.2.2 [Data Types](#data-types)
      * 3.2.2.1 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Int8](#rqsrs-003parameterizedqueriesdatatypeselectint8)
      * 3.2.2.2 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Int16](#rqsrs-003parameterizedqueriesdatatypeselectint16)
      * 3.2.2.3 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Int32](#rqsrs-003parameterizedqueriesdatatypeselectint32)
      * 3.2.2.4 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Int64](#rqsrs-003parameterizedqueriesdatatypeselectint64)
      * 3.2.2.5 [RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt8](#rqsrs-003parameterizedqueriesdatatypeselectuint8)
      * 3.2.2.6 [RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt16](#rqsrs-003parameterizedqueriesdatatypeselectuint16)
      * 3.2.2.7 [RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt32](#rqsrs-003parameterizedqueriesdatatypeselectuint32)
      * 3.2.2.8 [RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt64](#rqsrs-003parameterizedqueriesdatatypeselectuint64)
      * 3.2.2.9 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32](#rqsrs-003parameterizedqueriesdatatypeselectfloat32)
      * 3.2.2.10 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.Inf](#rqsrs-003parameterizedqueriesdatatypeselectfloat32inf)
      * 3.2.2.11 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.NaN](#rqsrs-003parameterizedqueriesdatatypeselectfloat32nan)
      * 3.2.2.12 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64](#rqsrs-003parameterizedqueriesdatatypeselectfloat64)
      * 3.2.2.13 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.Inf](#rqsrs-003parameterizedqueriesdatatypeselectfloat64inf)
      * 3.2.2.14 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.NaN](#rqsrs-003parameterizedqueriesdatatypeselectfloat64nan)
      * 3.2.2.15 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal32](#rqsrs-003parameterizedqueriesdatatypeselectdecimal32)
      * 3.2.2.16 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal64](#rqsrs-003parameterizedqueriesdatatypeselectdecimal64)
      * 3.2.2.17 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal128](#rqsrs-003parameterizedqueriesdatatypeselectdecimal128)
      * 3.2.2.18 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String](#rqsrs-003parameterizedqueriesdatatypeselectstring)
      * 3.2.2.19 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String.ASCII](#rqsrs-003parameterizedqueriesdatatypeselectstringascii)
      * 3.2.2.20 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String.UTF8](#rqsrs-003parameterizedqueriesdatatypeselectstringutf8)
      * 3.2.2.21 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Unicode](#rqsrs-003parameterizedqueriesdatatypeselectstringunicode)
      * 3.2.2.22 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Binary](#rqsrs-003parameterizedqueriesdatatypeselectstringbinary)
      * 3.2.2.23 [RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Empty](#rqsrs-003parameterizedqueriesdatatypeselectstringempty)
      * 3.2.2.24 [RQ.SRS-003.ParameterizedQueries.DataType.Select.FixedString](#rqsrs-003parameterizedqueriesdatatypeselectfixedstring)
      * 3.2.2.25 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Date](#rqsrs-003parameterizedqueriesdatatypeselectdate)
      * 3.2.2.26 [RQ.SRS-003.ParameterizedQueries.DataType.Select.DateTime](#rqsrs-003parameterizedqueriesdatatypeselectdatetime)
      * 3.2.2.27 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Enum](#rqsrs-003parameterizedqueriesdatatypeselectenum)
      * 3.2.2.28 [RQ.SRS-003.ParameterizedQueries.DataType.Select.UUID](#rqsrs-003parameterizedqueriesdatatypeselectuuid)
      * 3.2.2.29 [RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv6](#rqsrs-003parameterizedqueriesdatatypeselectipv6)
      * 3.2.2.30 [RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv4](#rqsrs-003parameterizedqueriesdatatypeselectipv4)
      * 3.2.2.31 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable](#rqsrs-003parameterizedqueriesdatatypeselectnullable)
      * 3.2.2.32 [RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable.NULL](#rqsrs-003parameterizedqueriesdatatypeselectnullablenull)
* 4 [References](#references)

# Revision History

This document is stored in an electronic form using [Git] source control management software
hosted in a Gitlab repository.

All the updates are tracked using the [Git]'s revision history.

* Gitlab repository: https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/blob/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md
* Revision history: https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/commits/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md 

## Introduction

This specification describes [ClickHouse] ODBC driver requirements related to the 
support of parameterized queries as described in [SQL Statement Parameters].

## Parent Specification

This specification is a subordination specification of
[QA-SRS003 ClickHouse ODBC Driver - Software Requirements Specification]. 

## Requirements

### General

#### RQ.SRS-003.ParameterizedQueries
version: 1.0

ODBC driver SHALL support parameterized queries as described in [SQL Statement Parameters].

#### RQ.SRS-003.ParameterizedQueries.DataTypes
version: 1.0

The ODBC driver SHALL support using parameters for all applicable [ClickHouse] data types.

#### RQ.SRS-003.ParameterizedQueries.pyodbc
version: 1.0

ODBC driver SHALL support executing parameterized queries using [pyodbc] connector.

#### RQ.SRS-003.ParameterizedQueries.unixODBC.isql
version: 1.0

ODBC driver SHALL support executing parameterized queries using [isql] connector
from [unixODBC] project.

#### RQ.SRS-003.ParameterizedQueries.unixODBC.iusql
version: 1.0

ODBC driver SHALL support executing parameterized queries using [iusql] connector
from [unixODBC] project.

#### RQ.SRS-003.ParameterizedQueries.iODBC.iodbctest
version: 1.0

ODBC driver SHALL support executing parameterized queries using [iodbctest] connector
from [iODBC] project.

#### RQ.SRS-003.ParameterizedQueries.iODBC.iodbctestw
version: 1.0

ODBC driver SHALL support executing parameterized queries using [iodbctestw] connector
from [iODBC] project.

### Specific

#### Syntax

##### RQ.SRS-003.ParameterizedQueries.Syntax.Select.Parameters
version: 1.0

ODBC driver SHALL support parameters in the `SELECT` statement using the syntax

    SELECT PartID, Description, Price FROM Parts WHERE PartID = ? AND Description = ? AND Price = ? 

#### Data Types

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Int8
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns 
with `Int8` data type having ranges `[-128 : 127]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Int16
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns 
with `Int16` data type having ranges `[-32768 : 32767]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Int32
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Int32` data type having ranges `[-2147483648 : 2147483647]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Int64
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Int64` data type having ranges `[-9223372036854775808 : 9223372036854775807]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt8
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `UInt8` data type having ranges `[0 : 255]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt16
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `UInt16` data type having ranges `[0 : 65535]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt32
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `UInt32` data type having ranges `[0 : 4294967295]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.UInt64
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `UInt64` data type having ranges `[0 : 18446744073709551615]`.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Float32` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.Inf
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns 
with `Float32` data type having value  
`Inf` (positive infinity) and `-Inf` (negative infinity).

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float32.NaN
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns 
with `Float32` data type having value `Nan` (not a number).

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Float64` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.Inf
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Float64` data type having value 
`Inf` (positive infinity) and `-Inf` (negative infinity).

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Float64.NaN
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Float64` data type having value `NaN` (not a number).

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal32
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Decimal32` data type having ranges
`[-(1 * 10^(9 - S)-(1 / (10^S))) : 1 * 10^(9 - S) - (1 / (10^S)]`

* **P** precision. Valid range: [ 1 : 38 ]. 
  Determines how many decimal digits number can have (including fraction).
* **S** scale. Valid range: [ 0 : P ]. 
  Determines how many decimal digits fraction can have.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal64
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns 
with `Decimal64` data type having ranges
`[-(1 * 10^(18 - S)-(1 / (10^S))) : 1 * 10^(18 - S) - (1 / (10^S)]`

* **P** precision. Valid range: [ 1 : 38 ]. 
  Determines how many decimal digits number can have (including fraction).
* **S** scale. Valid range: [ 0 : P ]. 
  Determines how many decimal digits fraction can have.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Decimal128
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Decimal128` data type having ranges
`[-(1 * 10^(38 - S)-(1 / (10^S))) : 1 * 10^(38 - S) - (1 / (10^S)]`

* **P** precision. Valid range: [ 1 : 38 ]. 
  Determines how many decimal digits number can have (including fraction).
* **S** scale. Valid range: [ 0 : P ]. 
  Determines how many decimal digits fraction can have.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String.ASCII
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type containing ASCII encoded strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String.UTF8
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type containing UTF-8 encoded strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Unicode
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type containing Unicode encoded strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Binary
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type containing binary data.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.String.Empty
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `String` data type containing empty value.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.FixedString
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `FixedString` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Date
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Date` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.DateTime
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `DateTime` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Enum
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Enum` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.UUID
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `UUID` data type and treat them like strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv6
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `IPv6` data type and treat them like strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.IPv4
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `IPv4` data type and treat them like strings.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by the columns
with `Nullable` data type.

##### RQ.SRS-003.ParameterizedQueries.DataType.Select.Nullable.NULL
version: 1.0

ODBC driver SHALL support using parameters for selecting columns and filtering by columns
with `Nullable` data type containing `NULL` value.

## References

* **SQL Statement Parameters:** https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/statement-parameters?view=sql-server-2017 
* **unixODBC:** http://www.unixodbc.org/
* **iODBC:** http://www.iodbc.org/
* **pyodbc:** https://github.com/mkleehammer/pyodbc/wiki
* **isql:** https://www.mankier.com/1/isql
* **iusql:** https://www.mankier.com/1/iusql
* **iodbctest:** https://www.mankier.com/1/iodbctest
* **iodbctestw:** https://www.mankier.com/1/iodbctestw 
* **ODBC:**  https://docs.microsoft.com/en-us/sql/odbc/reference/odbc-overview?view=sql-server-2017
* **SQL:** https://en.wikipedia.org/wiki/SQL
* **ClickHouse:** https://clickhouse.yandex
* **Gitlab repository:**: https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/blob/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md 
* **Revision history:** https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/commits/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md

[pyodbc]: https://github.com/mkleehammer/pyodbc/wiki
[isql]: https://www.mankier.com/1/isql
[iusql]: https://www.mankier.com/1/iusql
[iodbctest]: https://www.mankier.com/1/iodbctest
[iodbctestw]: https://www.mankier.com/1/iodbctestw
[unixODBC]: http://www.unixodbc.org/
[iODBC]: http://www.iodbc.org/
[ODBC]: https://docs.microsoft.com/en-us/sql/odbc/reference/odbc-overview?view=sql-server-2017 
[SQL]: https://en.wikipedia.org/wiki/SQL
[ClickHouse]: https://clickhouse.yandex
[Gitlab repository]: https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/blob/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md
[Revision history]: https://gitlab.com/altinity-qa/documents/qa-srs003-parameterizedqueries-clickhouse-odbc-driver/commits/master/QA_SRS003_ParameterizedQueries_ClickHouse_ODBC_Driver.md
[Git]: https://git-scm.com/
[SQL Statement Parameters]: https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/statement-parameters?view=sql-server-2017
[QA-SRS003 ClickHouse ODBC Driver - Software Requirements Specification]: https://gitlab.com/altinity-qa/documents/qa-srs003-clickhouse-odbc-driver/blob/master/QA_SRS003_ClickHouse_ODBC_Driver.md
