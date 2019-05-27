:: Install:
:: https://git-scm.com/download/win
:: https://www.visualstudio.com/downloads/ - Visual Studio Community 2019 - Select components:
::  + MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.21)
::  + Windows 10 SDK
::  + Windows Universal CRT SDK
::  + C++ 2019 Redistributable MSMs
:: Microsoft .NET Framework 3.5 : https://www.microsoft.com/en-us/download/details.aspx?id=21
:: http://wixtoolset.org/releases/ - "Download WIX X.XX.X" and "WiX Toolset Visual Studio 2019 Extension"

:: Also useful for tests:
:: https://msdn.microsoft.com/en-us/data/aa937730  - Microsoft Data Access Components (MDAC) 2.8 Software Development Kit

:: call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.18362.0

SET PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\
SET PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\

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
