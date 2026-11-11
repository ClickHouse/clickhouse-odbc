# ClickHouse ODBC Driver

Version: v@VERSION@
Platform: Windows 64-bit

## Purpose

This archive contains the extracted runtime payload from the official ClickHouse ODBC MSI package.

## Package contents

- `bin/` — ClickHouse ANSI and Unicode ODBC drivers and runtime DLLs
- `clickhouse-odbc.reg` — Windows Registry file for registering the ClickHouse ODBC drivers
- `SHA256SUMS` — SHA-256 checksums for all packaged files

## Deployment outline

1. Copy the contents of the archive to `C:\Program Files\clickhouse-odbc` or another directory.
2. If you choose a different directory, open `clickhouse-odbc.reg` in a text editor and modify the paths accordingly.
3. Apply the registry values.
4. Confirm that the drivers appear under the _Drivers_ tab in the _64-bit Windows ODBC Data Source Administrator_.
5. Create and test a DSN, or configure a DSN-less connection.
