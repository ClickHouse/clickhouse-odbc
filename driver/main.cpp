#include "platform.h"

#if defined (_win_)

#include <algorithm>

#include <odbcinst.h>
#pragma comment(lib, "odbc32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")

#ifndef INTFUNC
#   define INTFUNC  __stdcall
#endif /* INTFUNC */

#define MAXPGPATH					1024

#define MAXKEYLEN		(32+1)	/* Max keyword length */
#define	MAXDSNAME	(32+1)	/* Max data source name length */

HINSTANCE NEAR s_hModule;		/* Saved module handle. */

/* Globals */
/* NOTE:  All these are used by the dialog procedures */
typedef struct tagSETUPDLG
{
    HWND		hwndParent;		/* Parent window handle */
    LPCSTR		lpszDrvr;		/* Driver description */
    //ConnInfo	ci;
    char		szDSN[MAXDSNAME];		/* Original data source name */
    BOOL		fNewDSN;		/* New data source flag */
    BOOL		fDefault;		/* Default data source flag */

}	SETUPDLG, FAR * LPSETUPDLG;

extern "C" 
{

void INTFUNC
ParseAttributes(LPCSTR lpszAttributes, LPSETUPDLG lpsetupdlg)
{
#if 0
    LPCSTR		lpsz;
    LPCSTR		lpszStart;
    char		aszKey[MAXKEYLEN];
    int			cbKey;
    char		value[MAXPGPATH];

    CC_conninfo_init(&(lpsetupdlg->ci), COPY_GLOBALS);

    for (lpsz = lpszAttributes; *lpsz; lpsz++)
    {
        /*
        * Extract key name (e.g., DSN), it must be terminated by an
        * equals
        */
        lpszStart = lpsz;
        for (;; lpsz++)
        {
            if (!*lpsz)
                return;			/* No key was found */
            else if (*lpsz == '=')
                break;			/* Valid key found */
        }
        /* Determine the key's index in the key table (-1 if not found) */
        cbKey = lpsz - lpszStart;
        if (cbKey < sizeof(aszKey))
        {
            memcpy(aszKey, lpszStart, cbKey);
            aszKey[cbKey] = '\0';
        }

        /* Locate end of key value */
        lpszStart = ++lpsz;
        for (; *lpsz; lpsz++)
            ;

        /* lpsetupdlg->aAttr[iElement].fSupplied = TRUE; */
        memcpy(value, lpszStart, std::min<size_t>(lpsz - lpszStart + 1, MAXPGPATH));

        //mylog("aszKey='%s', value='%s'\n", aszKey, value);

        /* Copy the appropriate value to the conninfo  */
        if (!copyAttributes(&lpsetupdlg->ci, aszKey, value))
            copyCommonAttributes(&lpsetupdlg->ci, aszKey, value);
    }
#endif
}

BOOL INTFUNC
SetDSNAttributes(HWND hwndParent, LPSETUPDLG lpsetupdlg, DWORD *errcode)
{
    LPCSTR		lpszDSN;		/* Pointer to data source name */

    lpszDSN = "2";//lpsetupdlg->ci.dsn;

    if (errcode)
        *errcode = 0;
    /* Validate arguments */
    //if (lpsetupdlg->fNewDSN && !*lpsetupdlg->ci.dsn)
    //    return FALSE;

    /* Write the data source name */
    if (!SQLWriteDSNToIni(lpszDSN, lpsetupdlg->lpszDrvr))
    {
        RETCODE	ret = SQL_ERROR;
        DWORD	err = SQL_ERROR;
        char    szMsg[SQL_MAX_MESSAGE_LENGTH];

        ret = SQLInstallerError(1, &err, szMsg, sizeof(szMsg), NULL);
        if (hwndParent)
        {
            char		szBuf[MAXPGPATH];

            if (SQL_SUCCESS != ret)
            {
                //LoadString(s_hModule, IDS_BADDSN, szBuf, sizeof(szBuf));
                //wsprintf(szMsg, szBuf, lpszDSN);
            }
            //LoadString(s_hModule, IDS_MSGTITLE, szBuf, sizeof(szBuf));
            //MessageBox(hwndParent, szMsg, szBuf, MB_ICONEXCLAMATION | MB_OK);
        }
        if (errcode)
            *errcode = err;
        return FALSE;
    }

    /* Update ODBC.INI */
    //writeDriverCommoninfo(ODBC_INI, lpsetupdlg->ci.dsn, &(lpsetupdlg->ci.drivers));
    //writeDSNinfo(&lpsetupdlg->ci);

    /* If the data source name has changed, remove the old name */
    //if (lstrcmpi(lpsetupdlg->szDSN, lpsetupdlg->ci.dsn))
    //    SQLRemoveDSNFromIni(lpsetupdlg->szDSN);
    return TRUE;
}

BOOL CALLBACK
ConfigDSN(HWND hwnd,
          WORD fRequest,
          LPCSTR lpszDriver,
          LPCSTR lpszAttributes)
{
    BOOL fSuccess = FALSE;
    GLOBALHANDLE hglbAttr;
    LPSETUPDLG lpsetupdlg;

    /* Allocate attribute array */
    hglbAttr = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(SETUPDLG));
    if (!hglbAttr)
        return FALSE;
    lpsetupdlg = (LPSETUPDLG)GlobalLock(hglbAttr);
    /* Parse attribute string */
    if (lpszAttributes)
        ParseAttributes(lpszAttributes, lpsetupdlg);

    /* Save original data source name */
    //if (lpsetupdlg->ci.dsn[0])
    //    lstrcpy(lpsetupdlg->szDSN, lpsetupdlg->ci.dsn);
    //else
    //    lpsetupdlg->szDSN[0] = '\0';

    /* Remove data source */
    if (ODBC_REMOVE_DSN == fRequest)
    {
        /* Fail if no data source name was supplied */
        //if (!lpsetupdlg->ci.dsn[0])
        //    fSuccess = FALSE;

        /* Otherwise remove data source from ODBC.INI */
        //else
        //    fSuccess = SQLRemoveDSNFromIni(lpsetupdlg->ci.dsn);
    }
    /* Add or Configure data source */
    else
    {
        /* Save passed variables for global access (e.g., dialog access) */
        lpsetupdlg->hwndParent = hwnd;
        lpsetupdlg->lpszDrvr = lpszDriver;
        lpsetupdlg->fNewDSN = (ODBC_ADD_DSN == fRequest);
        //lpsetupdlg->fDefault = !lstrcmpi(lpsetupdlg->ci.dsn, INI_DSN);

        /*
        * Display the appropriate dialog (if parent window handle
        * supplied)
        */
        if (hwnd)
        {
            /* Display dialog(s) */
            fSuccess = true; /*(IDOK == DialogBoxParam(s_hModule,
                                               MAKEINTRESOURCE(DLG_CONFIG),
                                               hwnd,
                                               ConfigDlgProc,
                                               (LPARAM)lpsetupdlg)); */
        }
        //else if (lpsetupdlg->ci.dsn[0])
            fSuccess = SetDSNAttributes(hwnd, lpsetupdlg, NULL);
        //else
        //    fSuccess = TRUE;
    }

    GlobalUnlock(hglbAttr);
    GlobalFree(hglbAttr);

    return fSuccess;
}

BOOL CALLBACK
ConfigDriver(HWND hwnd,
             WORD fRequest,
             LPCSTR lpszDriver,
             LPCSTR lpszArgs,
             LPSTR lpszMsg,
             WORD cbMsgMax,
             WORD *pcbMsgOut)
{
    MessageBox(hwnd, "ConfigDriver", "Debug", MB_OK);
    return TRUE;
}

BOOL WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            s_hModule = (HINSTANCE)hInst;	/* Save for dialog boxes */
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return TRUE;
}

}

#endif 