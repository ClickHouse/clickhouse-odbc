:: Install:
:: https://git-scm.com/download/win
:: Microsoft .NET Framework 3.5 : Settings - System - Apps & features - Manage optional features - Add a feature - Microsoft .NET Framework 3.5  // <-- Check and fix
:: http://wixtoolset.org/releases/ - "Download WIX X.XX.X" and "WiX Toolset Visual Studio 2017 Extension"
:: https://www.visualstudio.com/downloads/ - Visual Studio Community 2017 - Desktop development with C++
::                                                                           + Windows 8.1 SDK and UCRT SDK

:: Also useful for tests:
:: https://msdn.microsoft.com/en-us/data/aa937730  - Microsoft Data Access Components (MDAC) 2.8 Software Development Kit



:: call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64 8.1

SET PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\
SET PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\

devenv /upgrade odbc64.sln
devenv /upgrade odbc32.sln

msbuild /m /p:Configuration=Release odbc64.sln || exit
msbuild /m /p:Configuration=Release odbc32.sln || exit
msbuild /m /p:Configuration=Debug odbc64.sln || exit
msbuild /m /p:Configuration=Debug odbc32.sln || exit
copy Debug\*.dll "C:\Program Files (x86)\ClickHouse ODBC\"
copy x64\Debug\*.dll "C:\Program Files\ClickHouse ODBC\"

:: installer64\bin\Debug\clickhouse_odbc_x64.msi /quiet
:: installer32\bin\Debug\clickhouse_odbc_x32.msi /quiet
:: installer64\bin\Release\clickhouse_odbc_x64.msi /quiet
:: installer32\bin\Release\clickhouse_odbc_x32.msi /quiet
