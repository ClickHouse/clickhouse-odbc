#!/usr/bin/env bash

set -Eeuo pipefail

VCVRANGE="${VCVRANGE:-(0,999)}"
VCVARSALL_ARGS="${VCVARSALL_ARGS:-x64}"

VSWHERE="${VSWHERE:-C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe}"
OLD_VSWHERE="${OLD_VSWHERE:-no}"

if [ -z "${VCVARSALL:-}" ]; then
    if [[ "$OLD_VSWHERE" == "yes" ]]; then
        VCVARSALL="$("${VSWHERE}" -nologo -utf8 -format value -products "*" -version "${VCVRANGE}" -property installationPath | grep -m1 "\S" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
        VCVARSALL="${VCVARSALL}/VC/Auxiliary/Build/vcvarsall.bat"
    else
        VCVARSALL="$("${VSWHERE}" -nologo -sort -utf8 -format value -products "*" -version "${VCVRANGE}" -find "**/vcvarsall.bat" | grep -m1 "\S" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
    fi
fi

VCVARSALL="${VCVARSALL//\\//}"

echo "\
call \"${VCVARSALL}\" ${VCVARSALL_ARGS}
if %errorlevel% neq 0 exit /b %errorlevel%
bash -Eeuxo pipefail -c \"$@\"
if %errorlevel% neq 0 exit /b %errorlevel%
" | cmd

exit $?
