
#include "utils.h"
//#include "platform.h"

#define	byte3check	0xfffff800
#define	byte2_base	0x80c0
#define	byte2_mask1	0x07c0
#define	byte2_mask2	0x003f
#define	byte3_base	0x8080e0
#define	byte3_mask1	0xf000
#define	byte3_mask2	0x0fc0
#define	byte3_mask3	0x003f


#define	surrog_check	0xfc00
#define	surrog1_bits	0xd800
#define	surrog2_bits	0xdc00
#define	byte4_base	0x808080f0
#define	byte4_sr1_mask1	0x0700
#define	byte4_sr1_mask2	0x00fc
#define	byte4_sr1_mask3	0x0003
#define	byte4_sr2_mask1	0x03c0
#define	byte4_sr2_mask2	0x003f
#define	surrogate_adjust	(0x10000 >> 10)

static int little_endian = -1;

SQLULEN	ucs2strlen(const SQLWCHAR *ucs2str)
{
	SQLULEN	len;
	for (len = 0; ucs2str[len]; len++)
		;
	return len;
}
char *ucs2_to_utf8(const SQLWCHAR *ucs2str, SQLLEN ilen, size_t *olen, bool lower_identifier)
{
	char *	utf8str;
	int	len = 0;
//MYLOG(0, "%p ilen=" FORMAT_LEN " ", ucs2str, ilen);

	if (!ucs2str)
	{
		if (olen)
			*olen = SQL_NULL_DATA;
		return NULL;
	}
	if (little_endian < 0)
	{
		int	crt = 1;
		little_endian = (0 != ((char *) &crt)[0]);
	}
	if (ilen < 0)
		ilen = ucs2strlen(ucs2str);
//MYPRINTF(0, " newlen=" FORMAT_LEN, ilen);
	utf8str = (char *) malloc(ilen * 4 + 1);
	if (utf8str)
	{
		int	i = 0;
		uint16_t	byte2code;
		int32_t	byte4code, surrd1, surrd2;
		const SQLWCHAR	*wstr;

		for (i = 0, wstr = ucs2str; i < ilen; i++, wstr++)
		{
			if (!*wstr)
				break;
			else if (0 == (*wstr & 0xffffff80)) /* ASCII */
			{
				if (lower_identifier)
					utf8str[len++] = (char) tolower(*wstr);
				else
					utf8str[len++] = (char) *wstr;
			}
			else if ((*wstr & byte3check) == 0)
			{
				byte2code = byte2_base |
					    ((byte2_mask1 & *wstr) >> 6) |
					    ((byte2_mask2 & *wstr) << 8);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte2code, sizeof(byte2code));
				else
				{
					utf8str[len] = ((char *) &byte2code)[1];
					utf8str[len + 1] = ((char *) &byte2code)[0];
				}
				len += sizeof(byte2code);
			}
			/* surrogate pair check for non ucs-2 code */
			else if (surrog1_bits == (*wstr & surrog_check))
			{
				surrd1 = (*wstr & ~surrog_check) + surrogate_adjust;
				wstr++;
				i++;
				surrd2 = (*wstr & ~surrog_check);
				byte4code = byte4_base |
					   ((byte4_sr1_mask1 & surrd1) >> 8) |
					   ((byte4_sr1_mask2 & surrd1) << 6) |
					   ((byte4_sr1_mask3 & surrd1) << 20) |
					   ((byte4_sr2_mask1 & surrd2) << 10) |
					   ((byte4_sr2_mask2 & surrd2) << 24);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, sizeof(byte4code));
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
					utf8str[len + 3] = ((char *) &byte4code)[0];
				}
				len += sizeof(byte4code);
			}
			else
			{
				byte4code = byte3_base |
					    ((byte3_mask1 & *wstr) >> 12) |
					    ((byte3_mask2 & *wstr) << 2) |
					    ((byte3_mask3 & *wstr) << 16);
				if (little_endian)
					memcpy(utf8str + len, (char *) &byte4code, 3);
				else
				{
					utf8str[len] = ((char *) &byte4code)[3];
					utf8str[len + 1] = ((char *) &byte4code)[2];
					utf8str[len + 2] = ((char *) &byte4code)[1];
				}
				len += 3;
			}
		}
		utf8str[len] = '\0';
		if (olen)
			*olen = len;
	}
//MYPRINTF(0, " olen=%d utf8str=%s\n", len, utf8str ? utf8str : "");
	return utf8str;
}
