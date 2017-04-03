#include "platform.h"
#include "resource.h"

#if defined (_win_)

#define strcpy strcpy_s
#define stricmp _stricmp

#include <algorithm>

#include <odbcinst.h>
#pragma comment(lib, "odbc32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")

#ifndef INTFUNC
#   define INTFUNC  __stdcall
#endif /* INTFUNC */

namespace
{

#define LARGE_REGISTRY_LEN			4096		/* used for special cases */
#define MEDIUM_REGISTRY_LEN			256 /* normal size for * user,database,etc. */
#define SMALL_REGISTRY_LEN			10	/* for 1/0 settings */

#define MAXPGPATH	1024
/// Max keyword length
#define MAXKEYLEN	(32+1)	
/// Max data source name length
#define	MAXDSNAME	(32+1)

#define INI_KDESC			"Description"	/* Data source description */
#define INI_DATABASE		"Database"	    /* Database Name */
#define INI_SERVER			"Servername"	/* Name of Server  running the ClickHouse service */
#define INI_UID				"UID"		    /* Default User Name */
#define INI_USERNAME		"Username"	    /* Default User Name */
#define INI_PASSWORD		"Password"	    /* Default Password */
#define INI_PORT			"Port"	        /* Port on which the ClickHouse is listening */
#define INI_READONLY		"ReadOnly"	    /* Database is read only */
#define INI_PROTOCOL		"Protocol"	    /* What protocol (6.2) */

#define ABBR_PROTOCOL		"A1"
#define ABBR_READONLY		"A0"

#define SPEC_SERVER			"server"

#ifndef WIN32
#define ODBC_INI			".odbc.ini"
#define ODBCINST_INI		"odbcinst.ini"
#else
#define ODBC_INI			"ODBC.INI"
#define ODBCINST_INI		"ODBCINST.INI"

#endif
/// Saved module handle.
HINSTANCE NEAR      s_hModule;		

/* Globals */


/*	Structure to hold all the connection attributes for a specific
connection (used for both registry and file, DSN and DRIVER)
*/
struct ConnInfo
{
    char		dsn[MEDIUM_REGISTRY_LEN];
    char		desc[MEDIUM_REGISTRY_LEN];
    char		drivername[MEDIUM_REGISTRY_LEN];
    char		server[MEDIUM_REGISTRY_LEN];
    char		database[MEDIUM_REGISTRY_LEN];
    char		username[MEDIUM_REGISTRY_LEN];
    char		password[MEDIUM_REGISTRY_LEN];
    char		port[SMALL_REGISTRY_LEN];
    char		sslmode[16];
    char		onlyread[SMALL_REGISTRY_LEN];
    char		fake_oid_index[SMALL_REGISTRY_LEN];
    char		show_oid_column[SMALL_REGISTRY_LEN];
    char		row_versioning[SMALL_REGISTRY_LEN];
    char		show_system_tables[SMALL_REGISTRY_LEN];
    char		translation_dll[MEDIUM_REGISTRY_LEN];
    char		translation_option[SMALL_REGISTRY_LEN];
    char		focus_password;
    char		conn_settings[MEDIUM_REGISTRY_LEN];
    signed char	disallow_premature = -1;
    signed char	allow_keyset = -1;
    signed char	updatable_cursors = 0;
    signed char	lf_conversion = -1;
    signed char	true_is_minus1 = -1;
    signed char	int8_as = -101;
    signed char	bytea_as_longvarbinary = -1;
    signed char	use_server_side_prepare = -1;
    signed char	lower_case_identifier = -1;
    signed char	rollback_on_error = -1;
    signed char	force_abbrev_connstr = -1;
    signed char	bde_environment = -1;
    signed char	fake_mss = -1;
    signed char	cvt_null_date_string = -1;
    signed char	autocommit_public = SQL_AUTOCOMMIT_ON;
    signed char	accessible_only = -1;
    signed char	ignore_round_trip_time = -1;
    signed char	disable_keepalive = -1;
    signed char	gssauth_use_gssapi = -1;
};

/* NOTE:  All these are used by the dialog procedures */
struct SetupDialogData
{
    HWND		hwnd_parent;	/// Parent window handle
    LPCSTR		driver_name;	/// Driver description
    ConnInfo	ci;
    char		dsn[MAXDSNAME]; /// Original data source name
    bool		is_new_dsn;		/// New data source flag
    bool		is_default;	    /// Default data source flag
};

BOOL
copyAttributes(ConnInfo *ci, const char * attribute, const char * value)
{
	BOOL	found = TRUE;

	if (stricmp(attribute, "DSN") == 0)
		strcpy(ci->dsn, value);

	else if (stricmp(attribute, "driver") == 0)
		strcpy(ci->drivername, value);

	else if (stricmp(attribute, INI_KDESC) == 0)
		strcpy(ci->desc, value);

	else if (stricmp(attribute, INI_DATABASE) == 0)
		strcpy(ci->database, value);

	else if (stricmp(attribute, INI_SERVER) == 0 || stricmp(attribute, SPEC_SERVER) == 0)
		strcpy(ci->server, value);

	else if (stricmp(attribute, INI_USERNAME) == 0 || stricmp(attribute, INI_UID) == 0)
		strcpy(ci->username, value);

	//else if (stricmp(attribute, INI_PASSWORD) == 0 || stricmp(attribute, "pwd") == 0)
	//	ci->password = decode_or_remove_braces(value);

	else if (stricmp(attribute, INI_PORT) == 0)
		strcpy(ci->port, value);

	else if (stricmp(attribute, INI_READONLY) == 0 || stricmp(attribute, ABBR_READONLY) == 0)
		strcpy(ci->onlyread, value);
#if 0
	else if (stricmp(attribute, INI_PROTOCOL) == 0 || stricmp(attribute, ABBR_PROTOCOL) == 0)
	{
		char * ptr;
		/*
		 * The first part of the Protocol used to be "6.2", "6.3" or
		 * "7.4" to denote which protocol version to use. Nowadays we
		 * only support the 7.4 protocol, also known as the protocol
		 * version 3. So just ignore the first part of the string,
		 * parsing only the rollback_on_error value.
		 */
		ptr = (char*)strchr(value, '-');
		if (ptr)
		{
			if ('-' != *value)
			{
				*ptr = '\0';
				/* ignore first part */
			}
			ci->rollback_on_error = atoi(ptr + 1);
		}
	}

	else if (stricmp(attribute, INI_SHOWOIDCOLUMN) == 0 || stricmp(attribute, ABBR_SHOWOIDCOLUMN) == 0)
		strcpy(ci->show_oid_column, value);

	else if (stricmp(attribute, INI_FAKEOIDINDEX) == 0 || stricmp(attribute, ABBR_FAKEOIDINDEX) == 0)
		strcpy(ci->fake_oid_index, value);

	else if (stricmp(attribute, INI_ROWVERSIONING) == 0 || stricmp(attribute, ABBR_ROWVERSIONING) == 0)
		strcpy(ci->row_versioning, value);

	else if (stricmp(attribute, INI_SHOWSYSTEMTABLES) == 0 || stricmp(attribute, ABBR_SHOWSYSTEMTABLES) == 0)
		strcpy(ci->show_system_tables, value);

	else if (stricmp(attribute, INI_CONNSETTINGS) == 0 || stricmp(attribute, ABBR_CONNSETTINGS) == 0)
	{
		/* We can use the conn_settings directly when they are enclosed with braces */
		if ('{' == *value)
		{
			size_t	len;

			len = strlen(value + 1);
			if (len > 0 && '}' == value[len])
				len--;
			STRN_TO_NAME(ci->conn_settings, value + 1, len);
		}
		else
			ci->conn_settings = decode(value);
	}
	else if (stricmp(attribute, INI_DISALLOWPREMATURE) == 0 || stricmp(attribute, ABBR_DISALLOWPREMATURE) == 0)
		ci->disallow_premature = atoi(value);
	else if (stricmp(attribute, INI_UPDATABLECURSORS) == 0 || stricmp(attribute, ABBR_UPDATABLECURSORS) == 0)
		ci->allow_keyset = atoi(value);
	else if (stricmp(attribute, INI_LFCONVERSION) == 0 || stricmp(attribute, ABBR_LFCONVERSION) == 0)
		ci->lf_conversion = atoi(value);
	else if (stricmp(attribute, INI_TRUEISMINUS1) == 0 || stricmp(attribute, ABBR_TRUEISMINUS1) == 0)
		ci->true_is_minus1 = atoi(value);
	else if (stricmp(attribute, INI_INT8AS) == 0)
		ci->int8_as = atoi(value);
	else if (stricmp(attribute, INI_BYTEAASLONGVARBINARY) == 0 || stricmp(attribute, ABBR_BYTEAASLONGVARBINARY) == 0)
		ci->bytea_as_longvarbinary = atoi(value);
	else if (stricmp(attribute, INI_USESERVERSIDEPREPARE) == 0 || stricmp(attribute, ABBR_USESERVERSIDEPREPARE) == 0)
		ci->use_server_side_prepare = atoi(value);
	else if (stricmp(attribute, INI_LOWERCASEIDENTIFIER) == 0 || stricmp(attribute, ABBR_LOWERCASEIDENTIFIER) == 0)
		ci->lower_case_identifier = atoi(value);
	else if (stricmp(attribute, INI_GSSAUTHUSEGSSAPI) == 0 || stricmp(attribute, ABBR_GSSAUTHUSEGSSAPI) == 0)
		ci->gssauth_use_gssapi = atoi(value);
	else if (stricmp(attribute, INI_KEEPALIVETIME) == 0 || stricmp(attribute, ABBR_KEEPALIVETIME) == 0)
		ci->keepalive_idle = atoi(value);
	else if (stricmp(attribute, INI_KEEPALIVEINTERVAL) == 0 || stricmp(attribute, ABBR_KEEPALIVEINTERVAL) == 0)
		ci->keepalive_interval = atoi(value);
#ifdef	USE_LIBPQ
	else if (stricmp(attribute, INI_PREFERLIBPQ) == 0 || stricmp(attribute, ABBR_PREFERLIBPQ) == 0)
		ci->prefer_libpq = atoi(value);
#endif /* USE_LIBPQ */
	else if (stricmp(attribute, INI_SSLMODE) == 0 || stricmp(attribute, ABBR_SSLMODE) == 0)
	{
		switch (value[0])
		{
			case SSLLBYTE_ALLOW:
				strcpy(ci->sslmode, SSLMODE_ALLOW);
				break;
			case SSLLBYTE_PREFER:
				strcpy(ci->sslmode, SSLMODE_PREFER);
				break;
			case SSLLBYTE_REQUIRE:
				strcpy(ci->sslmode, SSLMODE_REQUIRE);
				break;
			case SSLLBYTE_VERIFY:
				switch (value[1])
				{
					case 'f':
						strcpy(ci->sslmode, SSLMODE_VERIFY_FULL);
						break;
					case 'c':
						strcpy(ci->sslmode, SSLMODE_VERIFY_CA);
						break;
					default:
						strcpy(ci->sslmode, value);
				}
				break;
			case SSLLBYTE_DISABLE:
			default:
				strcpy(ci->sslmode, SSLMODE_DISABLE);
				break;
		}
	}
	else if (stricmp(attribute, INI_ABBREVIATE) == 0)
		unfoldCXAttribute(ci, value);
#ifdef	_HANDLE_ENLIST_IN_DTC_
	else if (stricmp(attribute, INI_XAOPT) == 0)
		ci->xa_opt = atoi(value);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	else if (stricmp(attribute, INI_EXTRAOPTIONS) == 0)
	{
		UInt4	val1 = 0, val2 = 0;

		if ('+' == value[0])
		{
			sscanf(value + 1, "%x-%x", &val1, &val2);
			add_removeExtraOptions(ci, val1, val2);
		}
		else if ('-' == value[0])
		{
			sscanf(value + 1, "%x", &val2);
			add_removeExtraOptions(ci, 0, val2);
		}
		else
		{
			setExtraOptions(ci, value, hex_format);
		}
	}
#endif
	else
		found = FALSE;

	return found;
}

static void parseAttributes(LPCSTR lpszAttributes, SetupDialogData * lpsetupdlg)
{
    LPCSTR		lpsz;
    LPCSTR		lpszStart;
    char		aszKey[MAXKEYLEN];
    int			cbKey;
    char		value[MAXPGPATH];

    //CC_conninfo_init(&(lpsetupdlg->ci), COPY_GLOBALS);

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

        /* Copy the appropriate value to the conninfo  */
        copyAttributes(&lpsetupdlg->ci, aszKey, value);

        //if (!copyAttributes(&lpsetupdlg->ci, aszKey, value))
        //    copyCommonAttributes(&lpsetupdlg->ci, aszKey, value);
    }
}

/*	This is for datasource based options only */
void writeDSNinfo(const ConnInfo * ci)
{
    const char * DSN = ci->dsn;
    //char encoded_item[LARGE_REGISTRY_LEN];
    //char temp[SMALL_REGISTRY_LEN];

    SQLWritePrivateProfileString(DSN,
                                 INI_KDESC,
                                 ci->desc,
                                 ODBC_INI);

    SQLWritePrivateProfileString(DSN,
                                 INI_DATABASE,
                                 ci->database,
                                 ODBC_INI);

    SQLWritePrivateProfileString(DSN,
                                 INI_SERVER,
                                 ci->server,
                                 ODBC_INI);

    SQLWritePrivateProfileString(DSN,
                                 INI_PORT,
                                 ci->port,
                                 ODBC_INI);

    SQLWritePrivateProfileString(DSN,
                                 INI_USERNAME,
                                 ci->username,
                                 ODBC_INI);
    SQLWritePrivateProfileString(DSN, INI_UID, ci->username, ODBC_INI);

    SQLWritePrivateProfileString(DSN,
                                 INI_READONLY,
                                 ci->onlyread,
                                 ODBC_INI);
}

static bool setDSNAttributes(HWND hwndParent, SetupDialogData * lpsetupdlg, DWORD * errcode)
{
    LPCSTR		lpszDSN;		/* Pointer to data source name */

    lpszDSN = lpsetupdlg->ci.dsn;

    if (errcode)
        *errcode = 0;
    /* Validate arguments */
    if (lpsetupdlg->is_new_dsn && !*lpsetupdlg->ci.dsn)
        return FALSE;

    /* Write the data source name */
    if (!SQLWriteDSNToIni(lpszDSN, lpsetupdlg->driver_name))
    {
        RETCODE	ret = SQL_ERROR;
        DWORD	err = SQL_ERROR;
        char    szMsg[SQL_MAX_MESSAGE_LENGTH];

        ret = SQLInstallerError(1, &err, szMsg, sizeof(szMsg), NULL);
        if (hwndParent)
        {
            //char		szBuf[MAXPGPATH];

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
    if (lstrcmpi(lpsetupdlg->dsn, lpsetupdlg->ci.dsn))
        SQLRemoveDSNFromIni(lpsetupdlg->dsn);

    return TRUE;
}

} // namespace 

extern "C" 
{

void INTFUNC
CenterDialog(HWND hdlg)
{
    HWND		hwndFrame;
    RECT		rcDlg,
        rcScr,
        rcFrame;
    int			cx,
        cy;

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
    if (rcDlg.bottom > rcScr.bottom)
    {
        rcDlg.bottom = rcScr.bottom;
        rcDlg.top = rcDlg.bottom - cy;
    }
    if (rcDlg.right > rcScr.right)
    {
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

LRESULT	CALLBACK
ConfigDlgProc(HWND hdlg,
              UINT wMsg,
              WPARAM wParam,
              LPARAM lParam)
{
    SetupDialogData * lpsetupdlg;
    ConnInfo * ci;
    //DWORD cmd;
    //char strbuf[64];

    switch (wMsg)
    {
        case WM_INITDIALOG:
        {
            lpsetupdlg = (SetupDialogData *)lParam;
            ci = &lpsetupdlg->ci;

            CenterDialog(hdlg); /* Center dialog */
            ShowWindow(hdlg, SW_SHOW);

            return FALSE;		/* Focus was not set */
        }
    }

    /* Message not processed */
    return FALSE;
}

BOOL CALLBACK
ConfigDSN(HWND hwnd,
          WORD fRequest,
          LPCSTR lpszDriver,
          LPCSTR lpszAttributes)
{
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

    /* Save original data source name */
    if (lpsetupdlg->ci.dsn[0])
        lstrcpy(lpsetupdlg->dsn, lpsetupdlg->ci.dsn);
    else
        lpsetupdlg->dsn[0] = '\0';

    /* Remove data source */
    if (ODBC_REMOVE_DSN == fRequest)
    {
        /* Fail if no data source name was supplied */
        if (!lpsetupdlg->ci.dsn[0])
            fSuccess = FALSE;

        /* Otherwise remove data source from ODBC.INI */
        else
            fSuccess = SQLRemoveDSNFromIni(lpsetupdlg->ci.dsn);
    }
    /* Add or Configure data source */
    else
    {
        /* Save passed variables for global access (e.g., dialog access) */
        lpsetupdlg->hwnd_parent = hwnd;
        lpsetupdlg->driver_name = lpszDriver;
        lpsetupdlg->is_new_dsn = (ODBC_ADD_DSN == fRequest);
        lpsetupdlg->is_default = true;//!lstrcmpi(lpsetupdlg->ci.dsn, INI_DSN);

        strcpy(lpsetupdlg->ci.dsn, "Test");
        strcpy(lpsetupdlg->ci.server, "localhost");
        strcpy(lpsetupdlg->ci.database, "default");
        strcpy(lpsetupdlg->ci.username, "defaul");
        strcpy(lpsetupdlg->ci.port, "8123");

        /*
        * Display the appropriate dialog (if parent window handle
        * supplied)
        */
        if (hwnd)
        {
            /* Display dialog(s) */
            fSuccess = setDSNAttributes(hwnd, lpsetupdlg, NULL);
            /*fSuccess = (IDOK == DialogBoxParam(s_hModule,
                                               MAKEINTRESOURCE(IDD_PROPPAGE_MEDIUM),
                                               hwnd,
                                               ConfigDlgProc,
                                               (LPARAM)lpsetupdlg));*/
        }
        else if (lpsetupdlg->ci.dsn[0])
            fSuccess = setDSNAttributes(hwnd, lpsetupdlg, NULL);
        else
            fSuccess = TRUE;
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