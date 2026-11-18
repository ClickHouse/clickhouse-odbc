---
name: issue-priority
description: >
  Decide the priority of a `ClickHouse/clickhouse-odbc` issue: `Urgent`,
  `High`, `Medium`, or `Low`, for the org issue field **Priority**. Use this
  skill when the task is to "prioritize", "set the priority of", "rate", or
  "triage" an issue, or to explain why an issue has a given priority. It
  defines the reach tiers of the driver, the severity rows, the base-level
  matrix, the modifiers, and the confidence.
---

# Issue priority

This skill is the single source of truth for the **Priority** of
`ClickHouse/clickhouse-odbc` issues. People and automated triage use the same
rules.

The value goes into the org issue field **Priority** (`Urgent`, `High`,
`Medium`, `Low`). The field is visible to org members only. An automated tool
sets the value once, never changes a value that is already set, and keeps the
level and its reason out of public comments.

## When to set no priority

Set no priority when one of these is true:

- A maintainer can close the issue with no further action: it is already fixed
  on `master`, a duplicate, works as intended, already supported, answered, or
  obsolete.
- The driver is not involved: `clickhouse-odbc-bridge`, the `odbc()` table
  function, and ODBC dictionaries. There, the ClickHouse server reads from
  another database through the ODBC driver of that database. That code is in
  `ClickHouse/ClickHouse`.
- The issue is a question, or it is about CI, the release process, or a
  discussion.

## Vulnerabilities that are not public yet

A security issue must not stay public with a description of how to reproduce
it. This section applies when a reproduction or code inspection confirms a
vulnerability that is not public yet. The classification skill of this
repository defines it
([`../issue-classification/SKILL.md`](../issue-classification/SKILL.md), "A
vulnerability that is not public yet"): memory corruption is one also when only
a hostile server or network causes it, and a hang, or a crash with no memory
corruption, that only such a server or network causes is not one.

- Report it in a private GitHub vulnerability report for this repository
  (**Security** → **Report a vulnerability**), never in a public issue,
  comment, or pull request. If a public issue reports it, copy the title, the
  text, and the comments of the issue into the report first.
- Set no Priority on the public issue. Close it with a comment that gives no
  details, and ask a repository administrator to delete it: closing does not
  hide the text of the issue.
- Rate the vulnerability with the rules below, and put the level and its reason
  into the private report as the proposed severity.

A vulnerability that is already public (a published CVE or GHSA, for example in
a dependency), and an authentication method that does not work or is missing,
get a Priority on the issue as usual.

## 1. Reach

The reach has two parts: the **path** (the driver function or feature) and the
**platform**. Take the tier of the most-used path that reaches the defect, and
the tier of the most-used platform that shows it. The reach is the narrower of
the two. Examples: a `SQLFetch` defect that shows only on macOS is Narrow. A
defect in a Narrow path is Narrow on every platform.

### Path

| Tier      | Public paths |
| --------- | ------------ |
| Core      | connect (`SQLDriverConnect`, `SQLConnect`, DSN and connection-string parsing of `Url`, `Server`/`Host`, `Port`, `UID`, `PWD`, `Database`, `SSLMode`); authentication (password, `AccessToken`); the HTTP and TLS transport (`driver/http_client.cpp`, `driver/connection.cpp`, the request code in `driver/statement.cpp`); `SQLExecDirect`, `SQLPrepare`, and `SQLExecute` with the default `ODBCDriver2` format; input parameters (`SQLBindParameter`); ODBC escape sequences (`{fn …}`, `{d …}`, `{ts …}`, `{oj …}`, `driver/escaping/`); `SQLFetch`, `SQLGetData`, `SQLBindCol`, and the conversion of common types (integers, `Float32`/`Float64`, `Decimal` (also `Decimal256`), `String`, `FixedString`, `Enum8`/`Enum16`, `Bool`, `UUID`, `Date`, `DateTime`, `DateTime64`, also in `Nullable` and `LowCardinality`); column metadata (`SQLNumResultCols`, `SQLDescribeCol`, `SQLColAttribute`); `SQLTables`, `SQLColumns`, `SQLGetTypeInfo`, `SQLGetInfo`; errors and diagnostics (`SQLGetDiagRec`, `SQLGetDiagField`); the ANSI and the Unicode driver; the Windows MSI installer |
| Common    | parameter arrays and data at execution (`SQL_ATTR_PARAMSET_SIZE`, `SQLPutData`, `SQLParamData`); `SQLMoreResults`, `SQLRowCount`; the other catalog functions (`SQLPrimaryKeys`, `SQLStatistics`, `SQLSpecialColumns`, `SQLForeignKeys`, `SQLProcedures`, the privilege functions); statement and connection attributes, timeouts, and `SQLCancel`; the optional DSN parameters (`Compression`, `AutoSessionId`, `HugeIntAsString`, `SqlCompatibilitySettings`, `ClientName`, `VerifyConnectionEarly`, `Timeout`, and the client-certificate parameters `PrivateKeyFile`, `CertificateFile`, `CALocation`); the Windows DSN setup dialog (`driver/platform/win/setup.cpp`); SQL that only one application other than Power BI generates (Excel, Tableau through generic ODBC, SQL Server linked servers, MS Access, Qlik, SSIS, Oracle); one connection or statement used from more than one thread |
| Narrow    | the `RowBinaryWithNamesAndTypes` format (experimental, `default_format` in the `Url`); rare types (`Int128` and wider, `Array`, `Map`, `Tuple`, `Nested`, `IPv4`, `IPv6`, Geo, `JSON`, `Dynamic`, `Variant`, `Time`, `Time64`); scrollable cursors and positioned operations (`SQLSetPos`, `SQLBulkOperations`, `SQLExtendedFetch`); driver logging (`DriverLog`, `DriverLogFile`); building the driver from sources; a ClickHouse server version that is no longer supported |
| Satellite | `test/`, `driver/test/`, `docs/`, `packaging/` samples (`odbc.ini.sample`, `odbcinst.ini.sample`, `clickhouse-odbc.tdc.sample`), `metrics/`, CI workflows |

Power BI, with the ClickHouse connector in DirectQuery mode, is the
application that sends the most reports. It generates SQL with parameters and
escape sequences, and it reads the type metadata. A defect that Power BI
reaches keeps the tier of its path.

A Power BI issue gets a priority, also when its root cause can be in the Power
BI connector (`ClickHouse/power-bi-clickhouse`): many issues that look like
connector issues are driver issues. Rate it by the driver path that Power BI
uses. When the root cause is in the connector, say in the reason that the fix
belongs in the connector.

### Platform

| Tier   | Platforms |
| ------ | --------- |
| Core   | Windows x64 with the Windows driver manager (MDAC/WDAC): the MSI and most BI tools; Linux x86_64 with UnixODBC: the Linux release package, servers, scripts |
| Narrow | macOS (the macOS release package, Homebrew; UnixODBC or iODBC): support is limited, and the macOS application sandbox stops most applications from loading the driver, so mainly software that its users build themselves uses it; Linux aarch64: not officially supported; builds that the project does not release: 32-bit Windows, Windows ARM64, iODBC on Linux, Cygwin or MinGW |

## 2. Severity

First name the **impacts** of the issue with the classification skill of this
repository
([`../issue-classification/SKILL.md`](../issue-classification/SKILL.md)): none,
one, or more of Security (part vulnerability or part authentication), Data
integrity, Stability, and Performance. Most issues have none.

Then use the first severity row that fits. With more than one impact, the
highest row applies:

| Severity                | Use when |
| ----------------------- | -------- |
| Wrong data or security  | impact **Data integrity**: the driver returns or stores wrong or incomplete data with no error, or reports success for a query that failed; or impact **Security** with a vulnerability: a security defect |
| Broken, no workaround   | the feature fails (an error, a hang, a crash, a leak that ends in a crash or in exhausted connections), and the caller has no reasonable workaround; also a wrong or incomplete error with no workaround: the failure is reported, but its SQLSTATE, native code, or message is wrong or lost. The impact **Stability** (a hang, a crash, such a leak) uses this row when the caller has no reasonable workaround |
| Workaround exists       | the feature fails, but the caller can avoid the failure with a small change (name the workaround in the reason); also impact **Performance** of a defect: a result that is correct but slow or uses too much memory (a request to make an operation faster is a feature request: last row) |
| Cosmetic, docs, feature | wrong log or message text, documentation, a feature request |

An issue that has the impact Security with no vulnerability (an authentication
method that does not work or is missing) does not use the first row. Rate an
authentication method that does not work by its failure ("Broken, no
workaround" or "Workaround exists"), and a request for a missing method as a
feature request.

A request to make an operation faster has the impact Performance, but it is a
feature request: use "Cosmetic, docs, feature". Only a defect with the impact
Performance (a correct result that is too slow) uses "Workaround exists".

A vulnerability in a dependency uses "Wrong data or security" when the client
calls the vulnerable code. When it does not, use "Cosmetic, docs, feature": for
a published one (a CVE or GHSA), the issue asks for a dependency update. One
that is not published yet stays private also then (see "Vulnerabilities that
are not public yet"), and this level is the proposed severity of the private
report.

Rules for the driver:

- **A workaround must be one that the user can apply.** A BI tool generates its
  own SQL, and its user cannot change it. For a defect in tool-generated SQL,
  a workaround counts only if a user of that tool can apply it: a DSN
  parameter that the user can set, a server setting, or another connector
  mode. A Power BI user cannot change a DSN parameter: only a new version of
  the connector can, and Microsoft must roll it out. So a DSN parameter is not
  a workaround for a Power BI user.
- **A crash ends the host application.** The driver runs inside the
  application (a BI tool, Excel, SQL Server, a script). A crash, an abort, or a
  hang is "Broken, no workaround" unless the user can avoid it with a small
  change.
- **Wrong metadata.** A wrong type, size, precision, or nullability from
  `SQLDescribeCol`, `SQLColAttribute`, `SQLColumns`, or `SQLGetTypeInfo` is
  "Wrong data" only when the issue shows that an application then returns or
  stores wrong values. When it makes an application fail, rate the failure.
  When the issue shows no effect, use "Workaround exists".

## 3. Base level

| Severity ↓ / Reach →    | Core   | Common | Narrow | Satellite |
| ----------------------- | ------ | ------ | ------ | --------- |
| Wrong data or security  | Urgent | High   | High   | Medium    |
| Broken, no workaround   | High   | Medium | Medium | Low       |
| Workaround exists       | Medium | Low    | Low    | Low       |
| Cosmetic, docs, feature | Low    | Low    | Low    | Low       |

In this repository the Common column is one level lower than in the other
client repositories (maintainer decision): only a Core path on a Core platform
gives `Urgent` without a modifier.

## 4. Modifiers

Apply them in this order:

1. **Security with a shown path from untrusted input.** The issue or a
   reproduction shows how a value that an application takes from its users
   reaches the defect: `Urgent` at any reach. Skip the other modifiers. A
   defect that puts a parameter value into SQL without quoting or escaping is a
   candidate: look for such a path. If none is shown, rate the defect by the
   failure that it shows. A vulnerability that is not public yet goes to a
   private vulnerability report, not to a public issue (see "Vulnerabilities
   that are not public yet").
2. **Regression in the latest published release**: up one level. This can
   give `Urgent`. It is a regression only when the latest release shows the
   problem and the release before it does not. Use releases, not
   pre-releases: `gh release list --repo ClickHouse/clickhouse-odbc
   --exclude-pre-releases`. The tags are `vX.Y.Z.YYYYMMDD`; some older tags
   have no `v` (for example `1.5.1.20251201`).

Customer escalations follow their own process: this skill has no modifier for
them.

## 5. Confidence

A missing reproduction lowers the confidence, not the level. Record the
confidence with the value when the tool that sets it supports it:

| Confidence | Use when |
| ---------- | -------- |
| `HIGH`     | a run shows the problem (or, for a feature, code inspection confirms its absence on `master`) |
| `MEDIUM`   | code inspection shows a defect, but no reproduction was run (for example, it needs Windows, a BI tool, or ClickHouse Cloud) |
| `LOW`      | the report lacks what a reproduction needs, or a run did not show the problem |

## 6. Reason

Write one line with the value:

`<tier> (<path>; <platform>) · <impacts, or "no impact"> · <severity> · <modifiers, or "no modifier"> → <level>.`

Name the workaround when the severity is "Workaround exists". When the
confidence is `LOW`, say what is missing.
