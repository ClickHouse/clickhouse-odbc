#define POCO_NO_UNWINDOWS

#include "driver/platform/platform.h"
#include "driver/platform/win/resource.h"
#include "driver/utils/utils.h"
#include "driver/config/config.h"
#include "driver/config/ini_defines.h"

#include <Poco/UTF8String.h>

#if defined(_win_)

#    include <algorithm>
#    include <strsafe.h>

#    include <odbcinst.h>
#    pragma comment(lib, "odbc32.lib")
#    pragma comment(lib, "legacy_stdio_definitions.lib")

#    ifndef INTFUNC
#        define INTFUNC __stdcall
#    endif /* INTFUNC */

/// Saved module handle.
extern HINSTANCE module_instance;

namespace {

#    define MAXPGPATH 1024
/// Max keyword length
#    define MAXKEYLEN (32 + 1)
/// Max data source name length
#    define MAXDSNAME (32 + 1)

#    define ABBR_PROTOCOL "A1"
#    define ABBR_READONLY "A0"

/* NOTE:  All these are used by the dialog procedures */
struct SetupDialogData {
    HWND hwnd_parent;    /// Parent window handle
    LPCTSTR driver_name; /// Driver description
    ConnInfo ci;
    TCHAR dsn[MAXDSNAME]; /// Original data source name
    bool is_new_dsn;      /// New data source flag
    bool is_default;      /// Default data source flag
};

BOOL copyAttributes(ConnInfo * ci, LPCTSTR attribute, LPCTSTR value) {
    const auto attribute_str = toUTF8(attribute);

#define COPY_ATTR_IF(NAME, INI_NAME)                          \
    if (Poco::UTF8::icompare(attribute_str, INI_NAME) == 0) { \
           ci->NAME = toUTF8(value);                          \
        return TRUE;                                          \
    }

    COPY_ATTR_IF(drivername, INI_DRIVER);
    COPY_ATTR_IF(dsn,        INI_DSN);
    COPY_ATTR_IF(desc,       INI_DESC);
    COPY_ATTR_IF(url,        INI_URL);
    COPY_ATTR_IF(server,     INI_SERVER);
    COPY_ATTR_IF(port,       INI_PORT);
    COPY_ATTR_IF(username,   INI_USERNAME);
    COPY_ATTR_IF(username,   INI_UID);
    COPY_ATTR_IF(password,   INI_PASSWORD);
    COPY_ATTR_IF(password,   INI_PWD);
    COPY_ATTR_IF(timeout,    INI_TIMEOUT);
    COPY_ATTR_IF(sslmode,    INI_SSLMODE);
    COPY_ATTR_IF(database,   INI_DATABASE);
    COPY_ATTR_IF(onlyread,   INI_READONLY);
    COPY_ATTR_IF(onlyread,   ABBR_READONLY);

#undef COPY_ATTR_IF

    return FALSE;
}

static void parseAttributes(LPCTSTR lpszAttributes, SetupDialogData * lpsetupdlg) {
    LPCTSTR lpsz;
    LPCTSTR lpszStart;
    TCHAR aszKey[MAXKEYLEN];
    int cbKey;
    TCHAR value[MAXPGPATH];

    for (lpsz = lpszAttributes; *lpsz; lpsz++) {
        /*
        * Extract key name (e.g., DSN), it must be terminated by an
        * equals
        */
        lpszStart = lpsz;
        for (;; lpsz++) {
            if (!*lpsz)
                return; /* No key was found */
            else if (*lpsz == '=')
                break; /* Valid key found */
        }
        /* Determine the key's index in the key table (-1 if not found) */
        cbKey = lpsz - lpszStart;
        if (cbKey < sizeof(aszKey)) {
            memcpy(aszKey, lpszStart, cbKey * sizeof(TCHAR));
            aszKey[cbKey] = '\0';
        }

        /* Locate end of key value */
        lpszStart = ++lpsz;
        for (; *lpsz; lpsz++)
            ;

        /* lpsetupdlg->aAttr[iElement].fSupplied = TRUE; */
        memcpy(value, lpszStart, std::min<size_t>(lpsz - lpszStart + 1, MAXPGPATH) * sizeof(TCHAR));

        /* Copy the appropriate value to the conninfo  */
        copyAttributes(&lpsetupdlg->ci, aszKey, value);

        //if (!copyAttributes(&lpsetupdlg->ci, aszKey, value))
        //    copyCommonAttributes(&lpsetupdlg->ci, aszKey, value);
    }
}

static bool setDSNAttributes(HWND hwndParent, SetupDialogData * lpsetupdlg, DWORD * errcode) {
    using CharType = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

    std::vector<CharType> DSN;
    fromUTF8(lpsetupdlg->ci.dsn, DSN);

    /// Pointer to data source name
    LPCTSTR lpszDSN = &DSN[0];

    if (errcode)
        *errcode = 0;

    /* Validate arguments */
    if (lpsetupdlg->is_new_dsn && lpsetupdlg->ci.dsn.empty())
        return FALSE;
    if (!SQLValidDSN(lpszDSN))
        return FALSE;

    /* Write the data source name */
    if (!SQLWriteDSNToIni(lpszDSN, lpsetupdlg->driver_name)) {
        RETCODE ret = SQL_ERROR;
        DWORD err = SQL_ERROR;
        TCHAR szMsg[SQL_MAX_MESSAGE_LENGTH];

        ret = SQLInstallerError(1, &err, szMsg, sizeof(szMsg), NULL);
        if (hwndParent) {
            if (SQL_SUCCESS != ret)
                MessageBox(hwndParent, szMsg, TEXT("Bad DSN configuration"), MB_ICONEXCLAMATION | MB_OK);
        }
        if (errcode)
            *errcode = err;
        return FALSE;
    }

    /* Update ODBC.INI */
    writeDSNinfo(&lpsetupdlg->ci);

    /* If the data source name has changed, remove the old name */
    if (Poco::UTF8::icompare(toUTF8(lpsetupdlg->dsn), lpsetupdlg->ci.dsn) != 0)
        SQLRemoveDSNFromIni(lpsetupdlg->dsn);

    return TRUE;
}

} // namespace

extern "C" {

void INTFUNC CenterDialog(HWND hdlg) {
    HWND hwndFrame;
    RECT rcDlg;
    RECT rcScr;
    RECT rcFrame;
    int cx;
    int cy;

    hwndFrame = GetParent(hdlg);

    GetWindowRect(hdlg, &rcDlg);
    cx = rcDlg.right - rcDlg.left;
    cy = rcDlg.bottom - rcDlg.top;

    GetClientRect(hwndFrame, &rcFrame);
    ClientToScreen(hwndFrame, (LPPOINT)(&rcFrame.left));
    ClientToScreen(hwndFrame, (LPPOINT)(&rcFrame.right));
    rcDlg.top = rcFrame.top + (((rcFrame.bottom - rcFrame.top) - cy) >> 1);
    rcDlg.left = rcFrame.left + (((rcFrame.right - rcFrame.left) - cx) >> 1);
    rcDlg.bottom = rcDlg.top + cy;
    rcDlg.right = rcDlg.left + cx;

    GetWindowRect(GetDesktopWindow(), &rcScr);
    if (rcDlg.bottom > rcScr.bottom) {
        rcDlg.bottom = rcScr.bottom;
        rcDlg.top = rcDlg.bottom - cy;
    }
    if (rcDlg.right > rcScr.right) {
        rcDlg.right = rcScr.right;
        rcDlg.left = rcDlg.right - cx;
    }

    if (rcDlg.left < 0)
        rcDlg.left = 0;
    if (rcDlg.top < 0)
        rcDlg.top = 0;

    MoveWindow(hdlg, rcDlg.left, rcDlg.top, cx, cy, TRUE);
    return;
}

INT_PTR CALLBACK ConfigDlgProc(HWND hdlg, UINT wMsg, WPARAM wParam, LPARAM lParam) {
    using CharType = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

    SetupDialogData * lpsetupdlg;
    ConnInfo * ci;
    //char strbuf[64];

    switch (wMsg) {
        case WM_INITDIALOG: {
            lpsetupdlg = (SetupDialogData *)lParam;
            ci = &lpsetupdlg->ci;
            SetWindowLongPtr(hdlg, DWLP_USER, lParam);

            CenterDialog(hdlg); /* Center dialog */

            getDSNinfo(ci, false);

            std::vector<CharType> value;

#define SET_DLG_ITEM(NAME, ID)                                \
    {                                                         \
        value.clear();                                        \
        fromUTF8(ci->NAME, value);                            \
        const auto res = SetDlgItemText(hdlg, ID, &value[0]); \
    }
            SET_DLG_ITEM(dsn, IDC_DSN_NAME);
            SET_DLG_ITEM(desc, IDC_DESCRIPTION);
            SET_DLG_ITEM(url, IDC_URL);
            SET_DLG_ITEM(server, IDC_SERVER_HOST);
            SET_DLG_ITEM(port, IDC_SERVER_PORT);
            SET_DLG_ITEM(database, IDC_DATABASE);
            SET_DLG_ITEM(username, IDC_USER);
            SET_DLG_ITEM(password, IDC_PASSWORD);
            SET_DLG_ITEM(timeout, IDC_TIMEOUT);
            SET_DLG_ITEM(sslmode, IDC_SSLMODE);

#undef SET_DLG_ITEM

            return TRUE; /* Focus was not set */
        }

        case WM_COMMAND:
            switch (const DWORD cmd = LOWORD(wParam)) {
                case IDOK: {
                    lpsetupdlg = (SetupDialogData *)GetWindowLongPtr(hdlg, DWLP_USER);
                    ci = &lpsetupdlg->ci;

                    std::vector<CharType> value;

#    define GET_DLG_ITEM(NAME, ID)                                               \
        {                                                                        \
            value.clear();                                                       \
            value.resize(MAX_DSN_VALUE_LEN);                                     \
            const auto read = GetDlgItemText(hdlg, ID, &value[0], value.size()); \
            value.resize(read <= 0 || read > value.size() ? 0 : read);           \
            ci->NAME = toUTF8(value);                                            \
        }

                    GET_DLG_ITEM(dsn, IDC_DSN_NAME);
                    GET_DLG_ITEM(desc, IDC_DESCRIPTION);
                    GET_DLG_ITEM(url, IDC_URL);
                    GET_DLG_ITEM(server, IDC_SERVER_HOST);
                    GET_DLG_ITEM(port, IDC_SERVER_PORT);
                    GET_DLG_ITEM(database, IDC_DATABASE);
                    GET_DLG_ITEM(username, IDC_USER);
                    GET_DLG_ITEM(password, IDC_PASSWORD);
                    GET_DLG_ITEM(timeout, IDC_TIMEOUT);
                    GET_DLG_ITEM(sslmode, IDC_SSLMODE);

#    undef GET_DLG_ITEM

                    /* Return to caller */
                }
                case IDCANCEL:
                    EndDialog(hdlg, cmd);
                    return TRUE;
            }
            break;
    }

    /* Message not processed */
    return FALSE;
}

BOOL INSTAPI EXPORTED_FUNCTION_MAYBE_W(ConfigDSN)(HWND hwnd, WORD fRequest, LPCTSTR lpszDriver, LPCTSTR lpszAttributes) {
    BOOL fSuccess = FALSE;
    GLOBALHANDLE hglbAttr;
    SetupDialogData * lpsetupdlg;

    /* Allocate attribute array */
    hglbAttr = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(SetupDialogData));
    if (!hglbAttr)
        return FALSE;
    lpsetupdlg = (SetupDialogData *)GlobalLock(hglbAttr);
    /* Parse attribute string */
    if (lpszAttributes)
        parseAttributes(lpszAttributes, lpsetupdlg);

    using CharType = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

    std::vector<CharType> DSN;
    fromUTF8(lpsetupdlg->ci.dsn, DSN);

    /// Pointer to data source name
    LPCTSTR lpszDSN = &DSN[0];

    /* Save original data source name */
    if (!lpsetupdlg->ci.dsn.empty())
        lstrcpy(lpsetupdlg->dsn, lpszDSN);
    else
        lpsetupdlg->dsn[0] = '\0';

    /* Remove data source */
    if (ODBC_REMOVE_DSN == fRequest) {
        /* Fail if no data source name was supplied */
        if (lpsetupdlg->ci.dsn.empty())
            fSuccess = FALSE;

        /* Otherwise remove data source from ODBC.INI */
        else
            fSuccess = SQLRemoveDSNFromIni(lpszDSN);
    }
    /* Add or Configure data source */
    else {
        /* Save passed variables for global access (e.g., dialog access) */
        lpsetupdlg->hwnd_parent = hwnd;
        lpsetupdlg->driver_name = lpszDriver;
        lpsetupdlg->is_new_dsn = (ODBC_ADD_DSN == fRequest);
        lpsetupdlg->is_default = (Poco::UTF8::icompare(lpsetupdlg->ci.dsn, INI_DSN_DEFAULT) == 0);

        /*
        * Display the appropriate dialog (if parent window handle
        * supplied)
        */
        if (hwnd) {
            /* Display dialog(s) */
            auto ret = DialogBoxParam(module_instance, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, ConfigDlgProc, (LPARAM)lpsetupdlg);
            if (ret == IDOK) {
                fSuccess = setDSNAttributes(hwnd, lpsetupdlg, NULL);
            } else if (ret != IDCANCEL) {
                auto err = GetLastError();
                LPVOID lpMsgBuf;
                LPVOID lpDisplayBuf;

                FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    err,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&lpMsgBuf,
                    0,
                    NULL);

                lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + 40) * sizeof(TCHAR));
                StringCchPrintf(
                    (LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("failed with error %d: %s"), err, lpMsgBuf);
                MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

                LocalFree(lpMsgBuf);
                LocalFree(lpDisplayBuf);
            }
        } else if (lpsetupdlg->ci.dsn[0])
            fSuccess = setDSNAttributes(hwnd, lpsetupdlg, NULL);
        else
            fSuccess = TRUE;
    }

    GlobalUnlock(hglbAttr);
    GlobalFree(hglbAttr);

    return fSuccess;
}

BOOL INSTAPI EXPORTED_FUNCTION_MAYBE_W(ConfigDriver)(
    HWND hwnd,
    WORD fRequest,
    LPCTSTR lpszDriver,
    LPCTSTR lpszArgs,
    LPTSTR lpszMsg,
    WORD cbMsgMax,
    WORD * pcbMsgOut
) {
    MessageBox(hwnd, TEXT("ConfigDriver"), TEXT("Debug"), MB_OK);
    return TRUE;
}

} // extern

#endif
