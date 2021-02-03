//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include <sys/stat.h>
#include <share.h>
#include <io.h>
#include <fcntl.h>
#include <regex>
#include "emule.h"
#include "OtherFunctions.h"
#include "DownloadQueue.h"
#include "Preferences.h"
#include "PartFile.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "UpDownClient.h"
#include "Opcodes.h"
#include "WebServices.h"
#include <shlobj.h>
#include "emuledlg.h"
#include "MenuCmds.h"
#include "ZipFile.h"
#include "RarFile.h"
#include <atlbase.h>
#include "StringConversion.h"
#include "shahashset.h"
#include "collection.h"
#include "SafeFile.h"
#include "Kademlia/Kademlia/kademlia.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "Log.h"
#include "CxImage/xImage.h"
#include "Netioapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


extern bool GetDRM(LPCTSTR pszFilePath);

CWebServices theWebServices;

// Base chars for encode and decode functions
static const byte base16Chars[17] = "0123456789ABCDEF";
#define BASE16_LOOKUP_MAX 23
static const byte base16Lookup[BASE16_LOOKUP_MAX][2] = {
	{ '0', 0x0 },
	{ '1', 0x1 },
	{ '2', 0x2 },
	{ '3', 0x3 },
	{ '4', 0x4 },
	{ '5', 0x5 },
	{ '6', 0x6 },
	{ '7', 0x7 },
	{ '8', 0x8 },
	{ '9', 0x9 },
	{ ':', 0x9 },
	{ ';', 0x9 },
	{ '<', 0x9 },
	{ '=', 0x9 },
	{ '>', 0x9 },
	{ '?', 0x9 },
	{ '@', 0x9 },
	{ 'A', 0xA },
	{ 'B', 0xB },
	{ 'C', 0xC },
	{ 'D', 0xD },
	{ 'E', 0xE },
	{ 'F', 0xF }
};
LPCTSTR const sBadFileNameChar(_T("\"*<>?|\\/:")); // lots of invalid chars for filenames in Windows :=)
LPCTSTR const sHiddenPassword = _T("\012\010\011\015\033"); //tough to enter from the keyboard

CString CastItoXBytes(uint16 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(uint32 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(uint64 count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

#if defined(_DEBUG) && defined(USE_DEBUG_EMFILESIZE)
CString CastItoXBytes(EMFileSize count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}
#endif

CString CastItoXBytes(float count, bool isK, bool isPerSec, uint32 decimal)
{
	return CastItoXBytes((double)count, isK, isPerSec, decimal);
}

CString CastItoXBytes(double count, bool isK, bool isPerSec, uint32 decimal)
{
	if (count <= 0.0) {
		UINT uid;
		if (isPerSec)
			uid = thePrefs.GetForceSpeedsToKB() ? IDS_KBYTESPERSEC : IDS_BYTESPERSEC;
		else
			uid = IDS_BYTES;
		return _T("0 ") + GetResString(uid);
	}
	if (isK) {
		if (count > 1.7E+300)
			count = 1.7E+300;
		else
			count *= 1024.0;
	}
	CString buffer;
	if (isPerSec) {
		if (thePrefs.GetForceSpeedsToKB())
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
		else if (count < 1024.0)
			buffer.Format(_T("%.0f %s"), count, (LPCTSTR)GetResString(IDS_BYTESPERSEC));
		else if (count < 1024000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
		else if (count < 1048576000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1048576.0, (LPCTSTR)GetResString(IDS_MBYTESPERSEC));
		else if (count < 1073741824000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1073741824.0, (LPCTSTR)GetResString(IDS_GBYTESPERSEC));
		else
			buffer.Format(_T("%.*f %s"), decimal, count / 1099511627776.0, (LPCTSTR)GetResString(IDS_TBYTESPERSEC));
	} else {
		if (count < 1024.0)
			buffer.Format(_T("%.0f %s"), count, (LPCTSTR)GetResString(IDS_BYTES));
		else if (count < 1024000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1024.0, (LPCTSTR)GetResString(IDS_KBYTES));
		else if (count < 1048576000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1048576.0, (LPCTSTR)GetResString(IDS_MBYTES));
		else if (count < 1073741824000.0)
			buffer.Format(_T("%.*f %s"), decimal, count / 1073741824.0, (LPCTSTR)GetResString(IDS_GBYTES));
		else
			buffer.Format(_T("%.*f %s"), decimal, count / 1099511627776.0, (LPCTSTR)GetResString(IDS_TBYTES));
	}
	return buffer;
}

CString CastItoIShort(uint16 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(uint32 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(uint64 count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(float count, bool isK, uint32 decimal)
{
	return CastItoIShort((double)count, isK, decimal);
}

CString CastItoIShort(double count, bool isK, uint32 decimal)
{
	if (count <= 0.0)
		return _T("0");

	if (isK)
		if (count > 1.7e+300)
			count = 1.7e+300;
		else
			count *= 1000.0;

	CString output;
	if (count < 1.0e15) {
		if (count < 1.0e3)
			output.Format(_T("%.0f"), count);
		else {
			UINT uid;
			double div;
			if (count < 1.0e6) {
				div = 1.0e3;
				uid = IDS_KILO;
			} else if (count < 1.0e9) {
				div = 1.0e6;
				uid = IDS_MEGA;
			} else if (count < 1.0e12) {
				div = 1.0e9;
				uid = IDS_GIGA;
			} else {
				div = 1.0e12;
				uid = IDS_TERA;
			}
			output.Format(_T("%.*f %s"), decimal, count / div, (LPCTSTR)GetResString(uid));
		}
	}
	return output;
}

CString CastSecondsToHM(time_t tSeconds)
{
	if (tSeconds == -1)	// invalid or unknown time value
		return CString(_T('?'));

	CString buffer;
	UINT count = (UINT)tSeconds;
	if (count < MIN2S(1))
		buffer.Format(_T("%u %s"), count, (LPCTSTR)GetResString(IDS_SECS));
	else if (count < HR2S(1))
		buffer.Format(_T("%u:%02u %s"), count / MIN2S(1), count - MIN2S(count / MIN2S(1)), (LPCTSTR)GetResString(IDS_MINS));
	else if (count < DAY2S(1))
		buffer.Format(_T("%u:%02u %s"), count / HR2S(1), (count - HR2S(count / HR2S(1))) / MIN2S(1), (LPCTSTR)GetResString(IDS_HOURS));
	else {
		UINT cntDays = count / DAY2S(1);
		UINT cntHrs = (count - DAY2S(cntDays)) / HR2S(1);
		buffer.Format(_T("%u %s %u %s"), cntDays, (LPCTSTR)GetResString(IDS_DAYS), cntHrs, (LPCTSTR)GetResString(IDS_HOURS));
	}
	return buffer;
}

CString CastSecondsToLngHM(time_t tSeconds)
{
	if (tSeconds == -1) // invalid or unknown time value
		return CString(_T('?'));

	CString buffer;
	UINT count = (UINT)tSeconds;
	if (count < MIN2S(1))
		buffer.Format(_T("%u %s"), count, (LPCTSTR)GetResString(IDS_LONGSECS));
	else if (count < HR2S(1))
		buffer.Format(_T("%u:%02u %s"), count / MIN2S(1), count - MIN2S(count / MIN2S(1)), (LPCTSTR)GetResString(IDS_LONGMINS));
	else if (count < DAY2S(1))
		buffer.Format(_T("%u:%02u %s"), count / HR2S(1), (count - HR2S(count / HR2S(1))) / MIN2S(1), (LPCTSTR)GetResString(IDS_LONGHRS));
	else {
		UINT cntDays = count / DAY2S(1);
		UINT cntHrs = (count - DAY2S(cntDays)) / HR2S(1);
		if (cntHrs)
			buffer.Format(_T("%u %s %u:%02u %s"), cntDays, (LPCTSTR)GetResString(IDS_DAYS2), cntHrs, (count - DAY2S(cntDays) - HR2S(cntHrs)) / MIN2S(1), (LPCTSTR)GetResString(IDS_LONGHRS));
		else
			buffer.Format(_T("%u %s %u %s"), cntDays, (LPCTSTR)GetResString(IDS_DAYS2), (count - DAY2S(cntDays) - HR2S(cntHrs)) / MIN2S(1), (LPCTSTR)GetResString(IDS_LONGMINS));
	}
	return buffer;
}

bool CheckFileOpen(LPCTSTR pszFilePath, LPCTSTR pszFileTitle)
{
	if (thePrefs.GetCheckFileOpen() && !PathIsURL(pszFilePath) && GetDRM(pszFilePath)) {
		CString strWarning;
		strWarning.Format(GetResString(IDS_FILE_WARNING_DRM), pszFileTitle != NULL ? pszFileTitle : pszFilePath);
		if (AfxMessageBox(strWarning, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2) != IDYES)
			return false;
	}
	return true;
}

HINSTANCE BrowserOpen(LPCTSTR lpURL, LPCTSTR lpDirectory)
{
	return ShellExecute(NULL, NULL, lpURL, NULL, lpDirectory, SW_SHOWDEFAULT);
}

void ShellOpen(LPCTSTR lpName, LPCTSTR lpParameters)
{
	ShellExecute(NULL, _T("open"), lpName, lpParameters, NULL, SW_SHOW);
}

void ShellOpenFile(LPCTSTR lpName)
{
	ShellOpen(lpName, NULL);
}

void ShellDefaultVerb(LPCTSTR lpName)
{
	ShellExecute(NULL, NULL, lpName, NULL, NULL, SW_SHOW);
}

bool ShellDeleteFile(LPCTSTR pszFilePath)
{
	if (!PathFileExists(pszFilePath))
		return true;
	if (thePrefs.GetRemoveToBin()) {
		TCHAR todel[MAX_PATH + 1] = {};
		_tcsncpy(todel, pszFilePath, _countof(todel) - 2);

		SHFILEOPSTRUCT fp = {};
		fp.wFunc = FO_DELETE;
		fp.hwnd = theApp.emuledlg->m_hWnd;
		fp.pFrom = todel;
		fp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NORECURSION;
		__try {
			return (SHFileOperation(&fp) == 0);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
		}
	}
	return (::DeleteFile(pszFilePath) != FALSE);
}

CString ShellGetFolderPath(int iCSIDL)
{
	CString strFolderPath;

	// Try the Unicode version from "shell32" *and* examine the function result - just the presence of that
	// function does not mean that it returns the requested path.
	//
	// Win98: 'SHGetFolderPathW' is available in 'shell32.dll', but it does not support all of the CSIDL values.
	HRESULT(WINAPI *pfnSHGetFolderPathW)(HWND, int, HANDLE, DWORD, LPWSTR);
	(FARPROC&)pfnSHGetFolderPathW = GetProcAddress(GetModuleHandle(_T("shell32")), "SHGetFolderPathW");
	if (pfnSHGetFolderPathW) {
		WCHAR wszPath[MAX_PATH];
		if ((*pfnSHGetFolderPathW)(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, wszPath) == S_OK)
			strFolderPath = wszPath;
	}

	if (strFolderPath.IsEmpty()) {
		HMODULE hLibShFolder = LoadLibrary(_T("shfolder.dll"));
		if (hLibShFolder) {
			(FARPROC&)pfnSHGetFolderPathW = GetProcAddress(hLibShFolder, "SHGetFolderPathW");
			if (pfnSHGetFolderPathW) {
				WCHAR wszPath[MAX_PATH];
				if ((*pfnSHGetFolderPathW)(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, wszPath) == S_OK)
					strFolderPath = wszPath;
			}

			if (strFolderPath.IsEmpty()) {
				HRESULT(WINAPI *pfnSHGetFolderPathA)(HWND, int, HANDLE, DWORD, LPSTR);
				(FARPROC&)pfnSHGetFolderPathA = GetProcAddress(hLibShFolder, "SHGetFolderPathA");
				if (pfnSHGetFolderPathA) {
					CHAR aszPath[MAX_PATH];
					if ((*pfnSHGetFolderPathA)(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, aszPath) == S_OK)
						strFolderPath = CString(aszPath);
				}
			}
			FreeLibrary(hLibShFolder);
		}
	}

	return strFolderPath;
}

namespace
{
	bool IsHexDigit(int c)
	{
		switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			return true;
		}
		return false;
	}
}

// Print the hash in a format which is similar to CertMgr's.
CString GetCertHash(const unsigned char *pucHash, int iBytes)
{
	CString strHash;
	LPTSTR pHash = strHash.GetBuffer(iBytes * 3);
	for (int i = 0; i < iBytes; ++i) {
		*pHash++ = base16Chars[*pucHash >> 4];
		*pHash++ = base16Chars[*pucHash++ & 0xf];
		*pHash++ = _T(' ');
	}
	pHash[-1] = _T('\0');
	strHash.ReleaseBuffer();
	return strHash.MakeLower();
}

// Print 'integer' in a format which is similar to CertMgr's.
CString GetCertInteger(const unsigned char *pucBlob, int cbBlob)
{
	CString strInteger;
	while (--cbBlob >= 0)
		strInteger.AppendFormat(_T("%02x "), pucBlob[cbBlob]);
	return strInteger.TrimRight();
}

CString URLDecode(const CString &inStr, bool bKeepNewLine)
{
	// decode escape sequences
	CString res;
	for (int x = 0; x < inStr.GetLength(); ++x) {
		if (inStr[x] == _T('%') && x + 2 < inStr.GetLength() && IsHexDigit(inStr[x + 1]) && IsHexDigit(inStr[x + 2])) {
			TCHAR hexstr[3];
			_tcsncpy(hexstr, inStr.Mid(x + 1, 2), 2);
			hexstr[2] = _T('\0');
			x += 2;

			// Convert the hex to ASCII
			TCHAR ch = (TCHAR)_tcstoul(hexstr, NULL, 16);
			if (ch > '\x1F' || (bKeepNewLine && ch == '\x0A')) // filter control chars
				res += ch;
		} else
			res += inStr[x];
	}
	return res;
}

CString URLEncode(const CString &sInT)
{
	CStringA sIn(sInT);
	LPCSTR pInBuf = sIn;

	CString sOut;
	LPTSTR pOutBuf = sOut.GetBuffer(sIn.GetLength() * 3);
	// do encoding
	while (*pInBuf) {
		if (_istalnum((BYTE)*pInBuf))
			*pOutBuf++ = (BYTE)*pInBuf;
		else {
			*pOutBuf++ = _T('%');
			*pOutBuf++ = toHex((BYTE)*pInBuf >> 4);
			*pOutBuf++ = toHex((BYTE)*pInBuf & 0xf);
		}
		++pInBuf;
	}
	*pOutBuf = _T('\0');
	sOut.ReleaseBuffer();
	return sOut;
}

CString EncodeURLQueryParam(const CString &sInT)
{
	CStringA sIn(sInT);
	LPCSTR pInBuf = sIn;

	// query		= *uric
	// uric			= reserved | unreserved | escaped
	// reserved		= ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
	// unreserved	= alphanum | mark
	// mark			= "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
	//
	// See also: http://www.w3.org/MarkUp/html-spec/html-spec_8.html

	CString sOut;
	LPTSTR pOutBuf = sOut.GetBuffer(sIn.GetLength() * 3);
	// do encoding
	while (*pInBuf) {
		if (_istalnum((BYTE)*pInBuf))
			*pOutBuf++ = (BYTE)*pInBuf;
		else if (_istspace((BYTE)*pInBuf))
			*pOutBuf++ = _T('+');
		else {
			*pOutBuf++ = _T('%');
			*pOutBuf++ = toHex((BYTE)*pInBuf >> 4);
			*pOutBuf++ = toHex((BYTE)*pInBuf & 0xf);
		}
		++pInBuf;
	}
	*pOutBuf = _T('\0');
	sOut.ReleaseBuffer();
	return sOut;
}

/*CString MakeStringEscaped(CString in)
{
	in.Replace(_T("&"), _T("&&"));
	return in;
}

CString RemoveAmpersand(const CString &rstr)
{
	CString str(rstr);
	str.Remove(_T('&'));
	return str;
}
*/
CString GetResNoAmp(RESSTRIDTYPE uStringID)
{
	CString str(GetResString(uStringID));
	str.Remove(_T('&'));
	return str;
}

bool Ask4RegFix(bool checkOnly, bool dontAsk, bool bAutoTakeCollections)
{
	// Barry - Make backup first
	if (!checkOnly)
		BackupReg();

	bool bGlobalSet = false;
	CRegKey regkey;
	LONG result;
	TCHAR modbuffer[MAX_PATH];
	DWORD dwModPathLen = ::GetModuleFileName(NULL, modbuffer, _countof(modbuffer));
	if (dwModPathLen == 0 || dwModPathLen == _countof(modbuffer))
		return false;
	CString strCanonFileName(modbuffer);
	strCanonFileName.Replace(_T("%"), _T("%%"));
	CString regbuffer;
	regbuffer.Format(_T("\"%s\" \"%%1\""), (LPCTSTR)strCanonFileName);

	// first check if the registry keys are already set (either by installer in HKLM or by user in HKCU)
	result = regkey.Open(HKEY_CLASSES_ROOT, _T("ed2k\\shell\\open\\command"), KEY_READ);
	if (result == ERROR_SUCCESS) {
		TCHAR rbuffer[MAX_PATH + 100];
		ULONG maxsize = _countof(rbuffer);
		regkey.QueryStringValue(NULL, rbuffer, &maxsize);
		rbuffer[_countof(rbuffer) - 1] = _T('\0');
		if (maxsize != 0 && _tcsicmp(rbuffer, regbuffer) == 0)
			bGlobalSet = true; // yup, globally we have an entry for this mule
		regkey.Close();
	}

	if (!bGlobalSet) {
		// we actually need to change the registry and write an entry for HKCU
		if (checkOnly)
			return true;
		HKEY hkeyCR = thePrefs.GetWindowsVersion() < _WINVER_2K_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
		if (regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\shell\\open\\command")) == ERROR_SUCCESS) {
			if (dontAsk || (LocMessageBox(IDS_ASSIGNED2K, MB_ICONQUESTION | MB_YESNO, 0) == IDYES)) {
				VERIFY(regkey.SetStringValue(NULL, regbuffer) == ERROR_SUCCESS);

				VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\DefaultIcon")) == ERROR_SUCCESS);
				VERIFY(regkey.SetStringValue(NULL, modbuffer) == ERROR_SUCCESS);

				VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k")) == ERROR_SUCCESS);
				VERIFY(regkey.SetStringValue(NULL, _T("URL: ed2k Protocol")) == ERROR_SUCCESS);
				VERIFY(regkey.SetStringValue(_T("URL Protocol"), _T("")) == ERROR_SUCCESS);

				VERIFY(regkey.Open(hkeyCR, _T("Software\\Classes\\ed2k\\shell\\open")) == ERROR_SUCCESS);
				regkey.RecurseDeleteKey(_T("ddexec"));
				regkey.RecurseDeleteKey(_T("ddeexec"));
			}
			regkey.Close();
		} else
			ASSERT(0);
	} else if (checkOnly)
		return bAutoTakeCollections && DoCollectionRegFix(true);
	else if (bAutoTakeCollections)
		DoCollectionRegFix(false);

	return false;
}

void BackupReg()
{
	// TODO: This function needs to be changed in at least 2 regards
	//	1)	It must follow the rules: reading from HKCR and writing into HKCU. What we are currently doing
	//		is not consistent with the other registry function which are dealing with the same keys.
	//
	//	2)	It behaves quite(!) differently under Win98 due to an obvious change in the Windows API.
	//		WinXP: Reading a non existent value returns 'key not found' error.
	//		Win98: Reading a non existent value returns an empty string (which gets saved and restored by our code).
	//		This means that saving/restoring existent registry keys works completely differently in Win98/XP.
	//		Actually it works correctly under Win98 and is broken in WinXP+. Though, did someone notice it at all?

	HKEY hkeyCR = thePrefs.GetWindowsVersion() < _WINVER_2K_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	// Look for pre-existing old ed2k links
	CRegKey regkey;
	if (regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\shell\\open\\command")) == ERROR_SUCCESS) {
		TCHAR rbuffer[MAX_PATH + 100];
		ULONG maxsize = _countof(rbuffer);
		if (regkey.QueryStringValue(_T("OldDefault"), rbuffer, &maxsize) != ERROR_SUCCESS || maxsize == 0) {
			maxsize = _countof(rbuffer);
			if (regkey.QueryStringValue(NULL, rbuffer, &maxsize) == ERROR_SUCCESS)
				VERIFY(regkey.SetStringValue(_T("OldDefault"), rbuffer) == ERROR_SUCCESS);

			VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\DefaultIcon")) == ERROR_SUCCESS);
			maxsize = _countof(rbuffer);
			if (regkey.QueryStringValue(NULL, rbuffer, &maxsize) == ERROR_SUCCESS)
				VERIFY(regkey.SetStringValue(_T("OldIcon"), rbuffer) == ERROR_SUCCESS);
		}
		regkey.Close();
	} else
		ASSERT(0);
}

// Barry - Restore previous values
void RevertReg()
{
	HKEY hkeyCR = thePrefs.GetWindowsVersion() < _WINVER_2K_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	// restore previous ed2k links before being assigned to emule
	CRegKey regkey;
	if (regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\shell\\open\\command")) == ERROR_SUCCESS) {
		TCHAR rbuffer[MAX_PATH + 100];
		ULONG maxsize = _countof(rbuffer);
		if (regkey.QueryStringValue(_T("OldDefault"), rbuffer, &maxsize) == ERROR_SUCCESS) {
			VERIFY(regkey.SetStringValue(NULL, rbuffer) == ERROR_SUCCESS);
			VERIFY(regkey.DeleteValue(_T("OldDefault")) == ERROR_SUCCESS);

			VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\ed2k\\DefaultIcon")) == ERROR_SUCCESS);
			maxsize = _countof(rbuffer);
			if (regkey.QueryStringValue(_T("OldIcon"), rbuffer, &maxsize) == ERROR_SUCCESS) {
				VERIFY(regkey.SetStringValue(NULL, rbuffer) == ERROR_SUCCESS);
				VERIFY(regkey.DeleteValue(_T("OldIcon")) == ERROR_SUCCESS);
			}
		}
		regkey.Close();
	} else
		ASSERT(0);
}

int GetMaxWindowsTCPConnections()
{
	return -1;
/*	OSVERSIONINFOEX osvi;
	osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
		//if OSVERSIONINFOEX doesn't work, try OSVERSIONINFO
		osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFO);
		if (!GetVersionEx((OSVERSIONINFO*)&osvi))
			return -1;  //shouldn't ever happen
	}

	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) // Windows NT product family
		return -1;  //no limits

	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) { // Windows 95 product family

		HKEY hKey;
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0) { //old school 95
			DWORD dwValue;
			DWORD dwLength = (DWORD)sizeof dwValue;

			RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\VxD\\MSTCP")
				, 0, KEY_QUERY_VALUE, &hKey);
			LONG lResult = RegQueryValueEx(hKey, TEXT("MaxConnections"), NULL, NULL
				, (LPBYTE)&dwValue, &dwLength);
			RegCloseKey(hKey);

			if (lResult != ERROR_SUCCESS || dwValue < 1)
				return 100;  //the default for 95 is 100

			return dwValue;

		}
		//98 or ME
		TCHAR szValue[32];
		DWORD dwLength = (DWORD)sizeof szValue;
		LONG lResult;

		RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\VxD\\MSTCP")
			, 0, KEY_QUERY_VALUE, &hKey);
		lResult = RegQueryValueEx(hKey, TEXT("MaxConnections"), NULL, NULL, (LPBYTE)szValue, &dwLength);
		RegCloseKey(hKey);

		LONG lMaxConnections;
		if (lResult != ERROR_SUCCESS || (lMaxConnections = _tstoi(szValue)) < 1)
			return 100;  //the default for 98/ME is 100

		return lMaxConnections;
	}

	return -1;  //give the user the benefit of the doubt, most use NT+ anyway
*/
}

#pragma warning(push)
#pragma warning(disable: 4996) //GetVersionEx()
WORD DetectWinVersion()
{
	OSVERSIONINFOEX osvi;
	osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
		osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFO);
		if (!GetVersionEx((OSVERSIONINFO*)&osvi))
			return 0;
	}

	switch (osvi.dwPlatformId) {
	case VER_PLATFORM_WIN32_NT:
		if (osvi.dwMajorVersion <= 4)
			return _WINVER_NT4_;
		if (osvi.dwMajorVersion == 5) {
			if (osvi.dwMinorVersion == 0)
				return _WINVER_2K_;
			if (osvi.dwMinorVersion == 1)
				return _WINVER_XP_;
			if (osvi.dwMinorVersion == 2)
				return _WINVER_2003_;
		}
		if (osvi.dwMajorVersion == 6) {
			if (osvi.dwMinorVersion == 0)
				return _WINVER_VISTA_;
			if (osvi.dwMinorVersion == 1)
				return _WINVER_7_;
			if (osvi.dwMinorVersion == 2)
				return _WINVER_8_;
			if (osvi.dwMinorVersion == 3)
				return _WINVER_8_1_;
		}
		if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0)
			return _WINVER_10_;
		return _WINVER_7_; // never return Win95 if we get the info about a NT system

	case VER_PLATFORM_WIN32_WINDOWS:
		if (osvi.dwMajorVersion == 4) {
			if (osvi.dwMinorVersion == 0)
				return _WINVER_95_;
			if (osvi.dwMinorVersion == 10)
				return _WINVER_98_;
			if (osvi.dwMinorVersion == 90)
				return _WINVER_ME_;
		}
	}

	return _WINVER_95_;		// there shouldn't be anything lower than this
}

bool IsRunningXPSP2()
{
	OSVERSIONINFOEX osvi;
	osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
		osvi.dwOSVersionInfoSize = (DWORD)sizeof(OSVERSIONINFO);
		if (!GetVersionEx((OSVERSIONINFO*)&osvi))
			return false;
	}

	return osvi.dwPlatformId == VER_PLATFORM_WIN32_NT
		&& osvi.dwMajorVersion == 5
		&& osvi.dwMinorVersion == 1
		&& osvi.wServicePackMajor >= 2;
}
#pragma warning(pop)

bool IsRunningXPSP2OrHigher()
{
	WORD wv = thePrefs.GetWindowsVersion();
	if (wv == _WINVER_XP_)
		return IsRunningXPSP2();
	return (wv > _WINVER_XP_);
}

uint64 GetFreeDiskSpaceX(LPCTSTR pDirectory)
{
	static BOOL _bInitialized = FALSE;
	static BOOL(WINAPI *_pfnGetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER) = NULL;

	if (!_bInitialized) {
		_bInitialized = TRUE;
		(FARPROC&)_pfnGetDiskFreeSpaceEx = GetProcAddress(GetModuleHandle(_T("kernel32")), _TWINAPI("GetDiskFreeSpaceEx"));
	}

	if (_pfnGetDiskFreeSpaceEx) {
		ULARGE_INTEGER nFreeDiskSpace;
		ULARGE_INTEGER dummy;
		if ((*_pfnGetDiskFreeSpaceEx)(pDirectory, &nFreeDiskSpace, &dummy, &dummy))
			return nFreeDiskSpace.QuadPart;
		return 0;
	}

	TCHAR cDrive[MAX_PATH];
	const TCHAR *p = _tcschr(pDirectory, _T('\\'));
	if (p) {
		size_t uChars = p - pDirectory;
		if (uChars >= _countof(cDrive))
			return 0;
		memcpy(cDrive, pDirectory, uChars * sizeof(TCHAR));
		cDrive[uChars] = _T('\0');
	} else {
		if (_tcslen(pDirectory) >= _countof(cDrive))
			return 0;
		_tcscpy(cDrive, pDirectory);
	}
	DWORD dwSectPerClust, dwBytesPerSect, dwFreeClusters, dwDummy;
	if (GetDiskFreeSpace(cDrive, &dwSectPerClust, &dwBytesPerSect, &dwFreeClusters, &dwDummy))
		return dwFreeClusters * (uint64)dwSectPerClust * (uint64)dwBytesPerSect;
	return 0;
}

CString GetRateString(UINT rate)
{
	static const UINT ids[6] =
	{
		IDS_CMT_NOTRATED, IDS_CMT_FAKE, IDS_CMT_POOR, IDS_CMT_FAIR, IDS_CMT_GOOD, IDS_CMT_EXCELLENT
	};

	return GetResString(ids[rate > 5 ? 0 : rate]);
}

// Returns a BASE32 encoded byte array
//
// [In]
//   buffer: Pointer to byte array
//   bufLen: Length of buffer array
//
// [Return]
//   CString object with BASE32 encoded byte array
CString EncodeBase32(const unsigned char *buffer, unsigned int bufLen)
{
	static const byte base32Chars[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	CString Base32Buff;

	unsigned index = 0;
	unsigned char word;

	for (unsigned i = 0; i < bufLen;) {

		// Is the current word going to span a byte boundary?
		if (index > 3) {
			word = (unsigned char)(buffer[i] & (0xFF >> index));
			index = (index + 5) % 8;
			word <<= index;
			if (i < bufLen - 1)
				word |= buffer[i + 1] >> (8 - index);

			++i;
		} else {
			word = (unsigned char)((buffer[i] >> (8 - (index + 5))) & 0x1F);
			index = (index + 5) % 8;
			if (index == 0)
				++i;
		}

		Base32Buff += (char)base32Chars[word];
	}

	return Base32Buff;
}

// Returns a BASE16 encoded byte array
//
// [In]
//   buffer: Pointer to byte array
//   bufLen: Length of the byte array
//
// [Return]
//   CString object with BASE16 encoded byte array
CString EncodeBase16(const unsigned char *buffer, unsigned int bufLen)
{
	CString Base16Buff;
	LPBYTE p = const_cast<LPBYTE>(buffer);
	LPTSTR bp = Base16Buff.GetBuffer(bufLen * 2);
	for (unsigned i = 0; i < bufLen; ++i) {
		*bp++ = (TCHAR)base16Chars[*p >> 4];
		*bp++ = (TCHAR)base16Chars[*p++ & 0xf];
	}
	Base16Buff.ReleaseBuffer(bufLen * 2);
	return Base16Buff;
}

// Decodes a BASE16 string into a byte array
//
// [In]
//   base16Buffer: String containing BASE16
//   base16BufLen: Length BASE16 coded string's length
//
// [Out]
//   buffer: byte array containing decoded string
bool DecodeBase16(const TCHAR *base16Buffer, unsigned int base16BufLen, byte *buffer, unsigned int bufflen)
{
	unsigned int uDecodeLengthBase16 = DecodeLengthBase16(base16BufLen);
	if (uDecodeLengthBase16 > bufflen)
		return false;
	memset(buffer, 0, uDecodeLengthBase16);

	for (unsigned int i = 0; i < base16BufLen; ++i) {
		int lookup = _totupper(base16Buffer[i]) - _T('0');

		// Check to make sure that the given word falls inside a valid range
		byte word = (lookup < 0 || lookup >= BASE16_LOOKUP_MAX) ? 0xFF : base16Lookup[lookup][1];

		if (i & 1)
			buffer[i / 2] |= word;
		else
			buffer[i / 2] = word << 4;
	}
	return true;
}

// Calculates length to decode from BASE16
//
// [In]
//   base16Length: Actual length of BASE16 string
//
// [Return]
//   New length of byte array decoded
unsigned DecodeLengthBase16(unsigned base16Length)
{
	return base16Length / 2U;
}

uint32 DecodeBase32(LPCTSTR pszInput, uchar *paucOutput, uint32 nBufferLen)
{
	if (pszInput == NULL)
		return 0;
	uint32 nInputLen = (uint32)_tcslen(pszInput);
	uint32 nDecodeLen = (nInputLen * 5 + 7) / 8;

	if (paucOutput == NULL || nBufferLen == 0)
		return nDecodeLen;
	if (nDecodeLen > nBufferLen)
		return 0;

	DWORD nBits = 0;
	int nCount = 0;

	for (int nChars = nInputLen; nChars-- > 0; ++pszInput) {
		if (*pszInput >= 'A' && *pszInput <= 'Z')
			nBits |= (*pszInput - 'A');
		else if (*pszInput >= 'a' && *pszInput <= 'z')
			nBits |= (*pszInput - 'a');
		else if (*pszInput >= '2' && *pszInput <= '7')
			nBits |= (*pszInput - '2' + 26);
		else
			return 0;

		nCount += 5;

		if (nCount >= 8) {
			*paucOutput++ = (BYTE)(nBits >> (nCount - 8));
			nCount -= 8;
		}

		nBits <<= 5;
	}

	return nDecodeLen;
}

uint32 DecodeBase32(LPCTSTR pszInput, CAICHHash &Hash)
{
	return DecodeBase32(pszInput, Hash.GetRawHash(), Hash.GetHashSize());
}

CWebServices::CWebServices()
{
	m_tDefServicesFileLastModified = 0;
}

CString CWebServices::GetDefaultServicesFile() const
{
	return thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("webservices.dat");
}

void CWebServices::RemoveAllServices()
{
	m_aServices.RemoveAll();
	m_tDefServicesFileLastModified = 0;
}

INT_PTR CWebServices::ReadAllServices()
{
	RemoveAllServices();

	const CString &strFilePath = GetDefaultServicesFile();
	FILE *readFile = _tfsopen(strFilePath, _T("r"), _SH_DENYWR);
	if (readFile != NULL) {
		CString sbuffer;
		while (!feof(readFile)) {
			TCHAR buffer[1024];
			if (_fgetts(buffer, _countof(buffer), readFile) == NULL)
				break;
			sbuffer = buffer;

			// ignore comments & too short lines
			if (sbuffer[0] == _T('#') || sbuffer[0] == _T('/') || sbuffer.GetLength() < 5)
				continue;

			int iPos = sbuffer.Find(_T(','));
			if (iPos > 0) {
				CString strUrlTemplate = sbuffer.Right(sbuffer.GetLength() - iPos - 1).Trim();
				if (!strUrlTemplate.IsEmpty()) {
					static LPCTSTR const _apszMacros[] = {
						_T("#hashid"),
						_T("#filesize"),
						_T("#filename"),
						_T("#name"),
						_T("#cleanfilename"),
						_T("#cleanname")
					};
					bool bFileMacros = false;
					for (int i = (int)_countof(_apszMacros); --i >= 0;)
						if (strUrlTemplate.Find(_apszMacros[i]) >= 0) {
							bFileMacros = true;
							break;
						}

					SEd2kLinkService svc;
					svc.uMenuID = (UINT)(MP_WEBURL + m_aServices.GetCount());
					svc.strMenuLabel = sbuffer.Left(iPos).Trim();
					svc.strUrl = strUrlTemplate;
					svc.bFileMacros = bFileMacros;
					m_aServices.Add(svc);
				}
			}
		}
		fclose(readFile);

		struct _stat64 st;
		if (statUTC(strFilePath, st) == 0)
			m_tDefServicesFileLastModified = (time_t)st.st_mtime;
	}

	return m_aServices.GetCount();
}

int CWebServices::GetAllMenuEntries(CTitleMenu *pMenu, DWORD dwFlags)
{
	struct _stat64 st;
	if (m_aServices.IsEmpty() || (statUTC(GetDefaultServicesFile(), st) == 0 && st.st_mtime > m_tDefServicesFileLastModified))
		ReadAllServices();

	int iMenuEntries = 0;
	for (INT_PTR i = 0; i < m_aServices.GetCount(); ++i) {
		const SEd2kLinkService &rSvc(m_aServices[i]);
		if ((dwFlags & WEBSVC_GEN_URLS) && rSvc.bFileMacros)
			continue;
		if ((dwFlags & WEBSVC_FILE_URLS) && !rSvc.bFileMacros)
			continue;
		if (pMenu->AppendMenu(MF_STRING, MP_WEBURL + i, rSvc.strMenuLabel, _T("WEB")))
			++iMenuEntries;
	}
	return iMenuEntries;
}

bool CWebServices::RunURL(const CAbstractFile *file, UINT uMenuID)
{
	for (int i = 0; i < m_aServices.GetCount(); ++i) {
		const SEd2kLinkService &rSvc(m_aServices[i]);
		if (rSvc.uMenuID == uMenuID) {
			CString strUrlTemplate = rSvc.strUrl;
			if (file != NULL) {
				// Convert hash to hexadecimal text and add it to the URL
				strUrlTemplate.Replace(_T("#hashid"), md4str(file->GetFileHash()));

				// Add file size to the URL
				CString temp;
				temp.Format(_T("%I64u"), (uint64)file->GetFileSize());
				strUrlTemplate.Replace(_T("#filesize"), temp);

				// add complete filename to the url
				strUrlTemplate.Replace(_T("#filename"), EncodeURLQueryParam(file->GetFileName()));

				// add basename to the url
				CString strBaseName = file->GetFileName();
				PathRemoveExtension(strBaseName.GetBuffer(strBaseName.GetLength()));
				strBaseName.ReleaseBuffer();
				strUrlTemplate.Replace(_T("#name"), EncodeURLQueryParam(strBaseName));

				// add cleaned up complete filename to the url
				strUrlTemplate.Replace(_T("#cleanfilename"), EncodeURLQueryParam(CleanupFilename(file->GetFileName())));

				// add cleaned up basename to the url
				strUrlTemplate.Replace(_T("#cleanname"), EncodeURLQueryParam(CleanupFilename(strBaseName, false)));
			}

			// Open URL
			TRACE(_T("Starting URL: %s\n"), (LPCTSTR)strUrlTemplate);
			return BrowserOpen(strUrlTemplate, thePrefs.GetMuleDirectory(EMULE_EXECUTABLEDIR)) > reinterpret_cast<HINSTANCE>(32);
		}
	}
	return false;
}

void CWebServices::Edit()
{
	CString sDat;
	sDat.Format(_T("\"%swebservices.dat\""), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_CONFIGDIR));
	ShellOpen(thePrefs.GetTxtEditor(), sDat);
}

typedef struct
{
	LPCTSTR	pszInitialDir;
	LPCTSTR	pszDlgTitle;
} BROWSEINIT, *LPBROWSEINIT;

extern "C" int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED) {
		// Set initial directory
		if (((LPBROWSEINIT)lpData)->pszInitialDir != NULL)
			SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM)((LPBROWSEINIT)lpData)->pszInitialDir);

		// Set dialog's window title
		if (((LPBROWSEINIT)lpData)->pszDlgTitle != NULL)
			SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)((LPBROWSEINIT)lpData)->pszDlgTitle);
	}

	return 0;
}

bool SelectDir(HWND hWnd, LPTSTR pszPath, LPCTSTR pszTitle, LPCTSTR pszDlgTitle)
{
	bool bResult = false;
	(void)CoInitialize(NULL);
	LPMALLOC pShlMalloc;
	if (SHGetMalloc(&pShlMalloc) == NOERROR) {
		BROWSEINFO BrsInfo = {};
		BrsInfo.hwndOwner = hWnd;
		BrsInfo.lpszTitle = (pszTitle != NULL) ? pszTitle : pszDlgTitle;
		BrsInfo.ulFlags = BIF_VALIDATE | BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS | BIF_SHAREABLE | BIF_DONTGOBELOWDOMAIN;

		BROWSEINIT BrsInit = {};
		if (pszPath != NULL || pszTitle != NULL || pszDlgTitle != NULL) {
			// Need the 'BrowseCallbackProc' to set those strings
			BrsInfo.lpfn = BrowseCallbackProc;
			BrsInfo.lParam = (LPARAM)&BrsInit;
			BrsInit.pszDlgTitle = (pszDlgTitle != NULL) ? pszDlgTitle : NULL/*pszTitle*/;
			BrsInit.pszInitialDir = pszPath;
		}

		LPITEMIDLIST pidlBrowse;
		if ((pidlBrowse = SHBrowseForFolder(&BrsInfo)) != NULL) {
			if (SHGetPathFromIDList(pidlBrowse, pszPath))
				bResult = true;
			pShlMalloc->Free(pidlBrowse);
		}
		pShlMalloc->Release();
	}
	CoUninitialize();
	return bResult;
}

void slosh(CString &path)
{
	if (path.Right(1) != _T("\\"))
		path += _T('\\');
}

void unslosh(CString &path)
{
	if (path.Right(1) == _T("\\"))
		path.Truncate(path.GetLength() - 1);
}

void canonical(CString &path)
{
	TCHAR szPath[MAX_PATH];
	if (PathCanonicalize(szPath, path))
		path = szPath;
}

void MakeFoldername(CString &rstrPath)
{
	if (!rstrPath.IsEmpty()) { // don't canonicalize an empty path, we would get a "\"
		canonical(rstrPath);
		unslosh(rstrPath);
	}
}

CString StringLimit(const CString &in, UINT length)
{
	if ((UINT)in.GetLength() <= length || length < 20)
		return in;
	return in.Left(length - 8) + _T("...") + in.Right(8);
}

BOOL DialogBrowseFile(CString &rstrPath, LPCTSTR pszFilters, LPCTSTR pszDefaultFileName, DWORD dwFlags, bool openfilestyle)
{
	CFileDialog myFileDialog(openfilestyle, NULL, pszDefaultFileName
		, dwFlags | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT
		, pszFilters
		, NULL //parent CWnd
		, 0); //MFC will automatically determine the size of OPENFILENAME structure
	if (myFileDialog.DoModal() != IDOK)
		return FALSE;
	rstrPath = myFileDialog.GetPathName();
	return TRUE;
}

void md4str(const byte *hash, TCHAR *pszHash)
{
	byte *p = const_cast<byte*>(hash);
	for (int i = 0; i < MDX_DIGEST_SIZE; ++i) {
		*pszHash++ = (TCHAR)base16Chars[*p >> 4];
		*pszHash++ = (TCHAR)base16Chars[*p++ & 0xf];
	}
	*pszHash = _T('\0');
}

CString md4str(const byte *hash)
{
	return EncodeBase16(hash, MDX_DIGEST_SIZE);
}

bool strmd4(const char *pszHash, byte *hash)
{
	for (int i = 0; i < MDX_DIGEST_SIZE; ++i) {
		char str[3];
		str[0] = pszHash[i * 2];
		if (!str[0])
			return false;
		str[1] = pszHash[i * 2 + 1];
		if (!str[1])
			return false;
		str[2] = '\0';
		unsigned b;
		if (sscanf(str, "%x", &b) != 1)
			return false;
		hash[i] = (byte)b;
	}
	return true;
}

bool strmd4(const CString &rstr, byte *hash)
{
	for (int i = 0; i < MDX_DIGEST_SIZE; ++i) {
		char str[3];
		str[0] = (char)rstr[i * 2];
		if (!str[0])
			return false;
		str[1] = (char)rstr[i * 2 + 1];
		if (!str[1])
			return false;
		str[2] = '\0';
		unsigned b;
		if (sscanf(str, "%x", &b) != 1)
			return false;
		hash[i] = (byte)b;
	}
	return true;
}

void StripTrailingColon(CString &rstr)
{
	int i = rstr.GetLength() - 1;
	if (i >= 0 && rstr[i] == _T(':'))
		rstr.Truncate(i);
}

CString ValidFilename(const CString &filename)
{
	CString fname = URLDecode(filename);
	if (fname.GetLength() > 100)
		fname.Truncate(100);
	// remove invalid characters
	for (LPCTSTR p = sBadFileNameChar; *p; )
		fname.Remove(*p++);
	return fname.Trim();
}

CString CleanupFilename(const CString &filename, bool bExtension)
{
	CString sClean(URLDecode(filename).MakeLower());

	//remove substrings, defined in the preferences (.ini)
	CString strlink(thePrefs.GetFilenameCleanups());
	strlink.MakeLower();

	for (int iPos = 0; iPos >= 0;) {
		const CString &sToken(strlink.Tokenize(_T("|"), iPos));
		if (!sToken.IsEmpty())
			sClean.Replace(sToken, _T(""));
	}

	// Replace "." with space - except the last one (extension-dot)
	int extpos = bExtension ? sClean.ReverseFind(_T('.')) : sClean.GetLength();
	for (int i = 0; i < extpos; ++i) {
		if (sClean[i] != _T('.'))
			continue;
		if (i <= 0 || i >= sClean.GetLength() - 1 || !_istdigit(sClean[i - 1]) || !_istdigit(sClean[i + 1]))
			sClean.SetAt(i, _T(' '));
	}

	// replace space-holders with spaces
	sClean.Replace(_T('_'), _T(' '));
	sClean.Replace(_T('+'), _T(' '));
	sClean.Replace(_T('='), _T(' '));

	// remove invalid characters
	for (LPCTSTR p = sBadFileNameChar; *p;)
		sClean.Remove(*p++);
	// remove [AD]
	int pos1 = -1;
	for (;;) {
		pos1 = sClean.Find(_T('['), pos1 + 1);
		if (pos1 < 0)
			break;
		int pos2 = sClean.Find(_T(']'), pos1);
		if (pos2 <= pos1)
			break;
		if (pos2 - pos1 > 1) {
			int numcount = pos2 - pos1 - 1;
			for (int i = numcount; i > 0; --i)
				if (_istdigit(sClean[pos1 + i]))
					numcount -= 2;

			if (numcount < 0)
				continue;
		}
		sClean.Delete(pos1, pos2 - pos1 + 1);
		--pos1;
	}

	// Make leading Caps
	if (sClean.GetLength() > 1) {
		sClean.SetAt(0, _totupper(sClean[0]));

		int topos = sClean.ReverseFind(_T('.')) - 1;
		if (topos < 0)
			topos = sClean.GetLength() - 1;

		for (int i = 0; i < topos; ++i)
			if (!_istalpha(sClean[i]) && sClean[i] != _T('\''))
				if (i >= sClean.GetLength() - 2 || !_istdigit(sClean[i + 2]))
					sClean.SetAt(i + 1, _totupper(sClean[i + 1]));
	}

	// additional formatting
	sClean.Replace(_T("()"), _T(""));
	sClean.Replace(_T("  "), _T(" "));
	sClean.Replace(_T(" ."), _T("."));
	sClean.Replace(_T("( "), _T("("));
	sClean.Replace(_T(" )"), _T(")"));
	sClean.Replace(_T("()"), _T(""));
	sClean.Replace(_T("{ "), _T("{"));
	sClean.Replace(_T(" }"), _T("}"));
	sClean.Replace(_T("{}"), _T(""));

	return sClean.Trim();
}

struct SED2KFileType
{
	LPCTSTR pszExt;
	EED2KFileType iFileType;
} g_aED2KFileTypes[] =
{
	{ _T(".aac"),   ED2KFT_AUDIO },		// Advanced Audio Coding File
	{ _T(".ac3"),   ED2KFT_AUDIO },		// Audio Codec 3 File
	{ _T(".aif"),   ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".aifc"),  ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".aiff"),  ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".amr"),   ED2KFT_AUDIO },		// Adaptive Multi-Rate Codec File
	{ _T(".ape"),   ED2KFT_AUDIO },		// Monkey's Audio Lossless Audio File
	{ _T(".au"),    ED2KFT_AUDIO },		// Audio File (Sun, Unix)
	{ _T(".aud"),   ED2KFT_AUDIO },		// General Audio File
	{ _T(".audio"), ED2KFT_AUDIO },		// General Audio File
	{ _T(".cda"),   ED2KFT_AUDIO },		// CD Audio Track
	{ _T(".dmf"),   ED2KFT_AUDIO },		// Delusion Digital Music File
	{ _T(".dsm"),   ED2KFT_AUDIO },		// Digital Sound Module
	{ _T(".dts"),   ED2KFT_AUDIO },		// DTS Encoded Audio File
	{ _T(".far"),   ED2KFT_AUDIO },		// Farandole Composer Module
	{ _T(".flac"),  ED2KFT_AUDIO },		// Free Lossless Audio Codec File
	{ _T(".it"),    ED2KFT_AUDIO },		// Impulse Tracker Module
	{ _T(".m1a"),   ED2KFT_AUDIO },		// MPEG-1 Audio File
	{ _T(".m2a"),   ED2KFT_AUDIO },		// MPEG-2 Audio File
	{ _T(".m4a"),   ED2KFT_AUDIO },		// MPEG-4 Audio File
	{ _T(".mdl"),   ED2KFT_AUDIO },		// DigiTrakker Module
	{ _T(".med"),   ED2KFT_AUDIO },		// Amiga MED Sound File
	{ _T(".mid"),   ED2KFT_AUDIO },		// MIDI File
	{ _T(".midi"),  ED2KFT_AUDIO },		// MIDI File
	{ _T(".mka"),   ED2KFT_AUDIO },		// Matroska Audio File
	{ _T(".mod"),   ED2KFT_AUDIO },		// Amiga Music Module File
	{ _T(".mp1"),   ED2KFT_AUDIO },		// MPEG-1 Audio File
	{ _T(".mp2"),   ED2KFT_AUDIO },		// MPEG-2 Audio File
	{ _T(".mp3"),   ED2KFT_AUDIO },		// MPEG-3 Audio File
	{ _T(".mpa"),   ED2KFT_AUDIO },		// MPEG Audio File
	{ _T(".mpc"),   ED2KFT_AUDIO },		// Musepack Compressed Audio File
	{ _T(".mtm"),   ED2KFT_AUDIO },		// MultiTracker Module
	{ _T(".ogg"),   ED2KFT_AUDIO },		// Ogg Vorbis Compressed Audio File
	{ _T(".psm"),   ED2KFT_AUDIO },		// Protracker Studio Module
	{ _T(".ptm"),   ED2KFT_AUDIO },		// PolyTracker Module
	{ _T(".ra"),    ED2KFT_AUDIO },		// Real Audio File
	{ _T(".rmi"),   ED2KFT_AUDIO },		// MIDI File
	{ _T(".s3m"),   ED2KFT_AUDIO },		// Scream Tracker 3 Module
	{ _T(".snd"),   ED2KFT_AUDIO },		// Audio File (Sun, Unix)
	{ _T(".stm"),   ED2KFT_AUDIO },		// Scream Tracker 2 Module
	{ _T(".umx"),   ED2KFT_AUDIO },		// Unreal Music Package
	{ _T(".wav"),   ED2KFT_AUDIO },		// WAVE Audio File
	{ _T(".wma"),   ED2KFT_AUDIO },		// Windows Media Audio File
	{ _T(".xm"),    ED2KFT_AUDIO },		// Fasttracker 2 Extended Module

	{ _T(".3g2"),   ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gp"),   ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gp2"),  ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gpp"),  ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".amv"),   ED2KFT_VIDEO },		// Anime Music Video File
	{ _T(".asf"),   ED2KFT_VIDEO },		// Advanced Systems Format File
	{ _T(".avi"),   ED2KFT_VIDEO },		// Audio Video Interleave File
	{ _T(".bik"),   ED2KFT_VIDEO },		// BINK Video File
	{ _T(".divx"),  ED2KFT_VIDEO },		// DivX-Encoded Movie File
	{ _T(".dvr-ms"),ED2KFT_VIDEO },		// Microsoft Digital Video Recording
	{ _T(".flc"),   ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".fli"),   ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".flic"),  ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".flv"),   ED2KFT_VIDEO },		// Flash Video File
	{ _T(".hdmov"), ED2KFT_VIDEO },		// High-Definition QuickTime Movie
	{ _T(".ifo"),   ED2KFT_VIDEO },		// DVD-Video Disc Information File
	{ _T(".m1v"),   ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".m2t"),   ED2KFT_VIDEO },		// MPEG-2 Video Transport Stream
	{ _T(".m2ts"),  ED2KFT_VIDEO },		// MPEG-2 Video Transport Stream
	{ _T(".m2v"),   ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".m4b"),   ED2KFT_VIDEO },		// MPEG-4 Video File
	{ _T(".m4v"),   ED2KFT_VIDEO },		// MPEG-4 Video File
	{ _T(".mkv"),   ED2KFT_VIDEO },		// Matroska Video File
	{ _T(".mov"),   ED2KFT_VIDEO },		// QuickTime Movie File
	{ _T(".movie"), ED2KFT_VIDEO },		// QuickTime Movie File
	{ _T(".mp1v"),  ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mp2v"),  ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".mp4"),   ED2KFT_VIDEO },		// MPEG-4 Video File
	{ _T(".mpe"),   ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".mpeg"),  ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".mpg"),   ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".mpv"),   ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".mpv1"),  ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mpv2"),  ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".ogm"),   ED2KFT_VIDEO },		// Ogg Media File
	{ _T(".pva"),   ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".qt"),    ED2KFT_VIDEO },		// QuickTime Movie
	{ _T(".ram"),   ED2KFT_VIDEO },		// Real Audio Media
	{ _T(".ratdvd"),ED2KFT_VIDEO },		// RatDVD Disk Image
	{ _T(".rm"),    ED2KFT_VIDEO },		// Real Media File
	{ _T(".rmm"),   ED2KFT_VIDEO },		// Real Media File
	{ _T(".rmvb"),  ED2KFT_VIDEO },		// Real Video Variable Bit Rate File
	{ _T(".rv"),    ED2KFT_VIDEO },		// Real Video File
	{ _T(".smil"),  ED2KFT_VIDEO },		// SMIL Presentation File
	{ _T(".smk"),   ED2KFT_VIDEO },		// Smacker Compressed Movie File
	{ _T(".swf"),   ED2KFT_VIDEO },		 // Macromedia Flash Movie
	{ _T(".tp"),    ED2KFT_VIDEO },		// Video Transport Stream File
	{ _T(".ts"),    ED2KFT_VIDEO },		// Video Transport Stream File
	{ _T(".vid"),   ED2KFT_VIDEO },		// General Video File
	{ _T(".video"), ED2KFT_VIDEO },		// General Video File
	{ _T(".vob"),   ED2KFT_VIDEO },		// DVD Video Object File
	{ _T(".vp6"),   ED2KFT_VIDEO },		// TrueMotion VP6 Video File
	{ _T(".wm"),    ED2KFT_VIDEO },		// Windows Media Video File
	{ _T(".wmv"),   ED2KFT_VIDEO },		// Windows Media Video File
	{ _T(".xvid"),  ED2KFT_VIDEO },		// Xvid-Encoded Video File

	{ _T(".bmp"),   ED2KFT_IMAGE },		// Bitmap Image File
	{ _T(".emf"),   ED2KFT_IMAGE },		// Enhanced Windows Metafile
	{ _T(".gif"),   ED2KFT_IMAGE },		// Graphical Interchange Format File
	{ _T(".ico"),   ED2KFT_IMAGE },		// Icon File
	{ _T(".jfif"),  ED2KFT_IMAGE },		// JPEG File Interchange Format
	{ _T(".jpe"),   ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".jpeg"),  ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".jpg"),   ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".pct"),   ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".pcx"),   ED2KFT_IMAGE },		// Paintbrush Bitmap Image File
	{ _T(".pic"),   ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".pict"),  ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".png"),   ED2KFT_IMAGE },		// Portable Network Graphic
	{ _T(".psd"),   ED2KFT_IMAGE },		// Photoshop Document
	{ _T(".psp"),   ED2KFT_IMAGE },		// Paint Shop Pro Image File
	{ _T(".tga"),   ED2KFT_IMAGE },		// Targa Graphic
	{ _T(".tif"),   ED2KFT_IMAGE },		// Tagged Image File
	{ _T(".tiff"),  ED2KFT_IMAGE },		// Tagged Image File
	{ _T(".wmf"),   ED2KFT_IMAGE },		// Windows Metafile
	{ _T(".wmp"),   ED2KFT_IMAGE },		// Windows Media Photo File
	{ _T(".xif"),   ED2KFT_IMAGE },		// ScanSoft Pagis Extended Image Format File

	{ _T(".7z"),    ED2KFT_ARCHIVE },	// 7-Zip Compressed File
	{ _T(".ace"),   ED2KFT_ARCHIVE },	// WinAce Compressed File
	{ _T(".alz"),   ED2KFT_ARCHIVE },	// ALZip Archive
	{ _T(".arc"),   ED2KFT_ARCHIVE },	// Compressed File Archive
	{ _T(".arj"),   ED2KFT_ARCHIVE },	// ARJ Compressed File Archive
	{ _T(".bz2"),   ED2KFT_ARCHIVE },	// Bzip Compressed File
	{ _T(".cab"),   ED2KFT_ARCHIVE },	// Cabinet File
	{ _T(".cbr"),   ED2KFT_ARCHIVE },	// Comic Book RAR Archive
	{ _T(".cbz"),   ED2KFT_ARCHIVE },	// Comic Book ZIP Archive
	{ _T(".gz"),    ED2KFT_ARCHIVE },	// Gnu Zipped File
	{ _T(".hqx"),   ED2KFT_ARCHIVE },	// BinHex 4.0 Encoded File
	{ _T(".lha"),   ED2KFT_ARCHIVE },	// LHARC Compressed Archive
	{ _T(".lzh"),   ED2KFT_ARCHIVE },	// LZH Compressed File
	{ _T(".msi"),   ED2KFT_ARCHIVE },	// Microsoft Installer File
	{ _T(".pak"),   ED2KFT_ARCHIVE },	// PAK (Packed) File
	{ _T(".par"),   ED2KFT_ARCHIVE },	// Parchive Index File
	{ _T(".par2"),  ED2KFT_ARCHIVE },	// Parchive 2 Index File
	{ _T(".rar"),   ED2KFT_ARCHIVE },	// WinRAR Compressed Archive
	{ _T(".sit"),   ED2KFT_ARCHIVE },	// Stuffit Archive
	{ _T(".sitx"),  ED2KFT_ARCHIVE },	// Stuffit X Archive
	{ _T(".tar"),   ED2KFT_ARCHIVE },	// Consolidated Unix File Archive
	{ _T(".tbz2"),  ED2KFT_ARCHIVE },	// Tar BZip 2 Compressed File
	{ _T(".tgz"),   ED2KFT_ARCHIVE },	// Gzipped Tar File
	{ _T(".xpi"),   ED2KFT_ARCHIVE },	// Mozilla Installer Package
	{ _T(".z"),     ED2KFT_ARCHIVE },	// Unix Compressed File
	{ _T(".zip"),   ED2KFT_ARCHIVE },	// Zipped File

	{ _T(".bat"),   ED2KFT_PROGRAM },	// Batch File
	{ _T(".cmd"),   ED2KFT_PROGRAM },	// Command File
	{ _T(".com"),   ED2KFT_PROGRAM },	// COM File
	{ _T(".exe"),   ED2KFT_PROGRAM },	// Executable File
	{ _T(".hta"),   ED2KFT_PROGRAM },	// HTML Application
	{ _T(".js"),    ED2KFT_PROGRAM },	// Java Script
	{ _T(".jse"),   ED2KFT_PROGRAM },	// Encoded  Java Script
	{ _T(".msc"),   ED2KFT_PROGRAM },	// Microsoft Common Console File
	{ _T(".vbe"),   ED2KFT_PROGRAM },	// Encoded Visual Basic Script File
	{ _T(".vbs"),   ED2KFT_PROGRAM },	// Visual Basic Script File
	{ _T(".wsf"),   ED2KFT_PROGRAM },	// Windows Script File
	{ _T(".wsh"),   ED2KFT_PROGRAM },	// Windows Scripting Host File

	{ _T(".bin"),   ED2KFT_CDIMAGE },	// CD Image
	{ _T(".bwa"),   ED2KFT_CDIMAGE },	// BlindWrite Disk Information File
	{ _T(".bwi"),   ED2KFT_CDIMAGE },	// BlindWrite CD/DVD Disc Image
	{ _T(".bws"),   ED2KFT_CDIMAGE },	// BlindWrite Sub Code File
	{ _T(".bwt"),   ED2KFT_CDIMAGE },	// BlindWrite 4 Disk Image
	{ _T(".ccd"),   ED2KFT_CDIMAGE },	// CloneCD Disk Image
	{ _T(".cue"),   ED2KFT_CDIMAGE },	// Cue Sheet File
	{ _T(".dmg"),   ED2KFT_CDIMAGE },	// Mac OS X Disk Image
	{ _T(".img"),   ED2KFT_CDIMAGE },	// Disk Image Data File
	{ _T(".iso"),   ED2KFT_CDIMAGE },	// Disc Image File
	{ _T(".mdf"),   ED2KFT_CDIMAGE },	// Media Disc Image File
	{ _T(".mds"),   ED2KFT_CDIMAGE },	// Media Descriptor File
	{ _T(".nrg"),   ED2KFT_CDIMAGE },	// Nero CD/DVD Image File
	{ _T(".sub"),   ED2KFT_CDIMAGE },	// Subtitle File
	{ _T(".toast"), ED2KFT_CDIMAGE },	// Toast Disc Image

	{ _T(".chm"),   ED2KFT_DOCUMENT },	// Compiled HTML Help File
	{ _T(".css"),   ED2KFT_DOCUMENT },	// Cascading Style Sheet
	{ _T(".diz"),   ED2KFT_DOCUMENT },	// Description in Zip File
	{ _T(".doc"),   ED2KFT_DOCUMENT },	// Document File
	{ _T(".dot"),   ED2KFT_DOCUMENT },	// Document Template File
	{ _T(".hlp"),   ED2KFT_DOCUMENT },	// Help File
	{ _T(".htm"),   ED2KFT_DOCUMENT },	// HTML File
	{ _T(".html"),  ED2KFT_DOCUMENT },	// HTML File
	{ _T(".nfo"),   ED2KFT_DOCUMENT },	// Warez Information File
	{ _T(".pdf"),   ED2KFT_DOCUMENT },	// Portable Document Format File
	{ _T(".pps"),   ED2KFT_DOCUMENT },	// PowerPoint Slide Show
	{ _T(".ppt"),   ED2KFT_DOCUMENT },	// PowerPoint Presentation
	{ _T(".ps"),    ED2KFT_DOCUMENT },	// PostScript File
	{ _T(".rtf"),   ED2KFT_DOCUMENT },	// Rich Text Format File
	{ _T(".text"),  ED2KFT_DOCUMENT },	// General Text File
	{ _T(".txt"),   ED2KFT_DOCUMENT },	// Text File
	{ _T(".wri"),   ED2KFT_DOCUMENT },	// Windows Write Document
	{ _T(".xls"),   ED2KFT_DOCUMENT },	// Microsoft Excel Spreadsheet
	{ _T(".xml"),   ED2KFT_DOCUMENT },	// XML File

	{ _T(".emulecollection"), ED2KFT_EMULECOLLECTION }
};

int __cdecl CompareE2DKFileType(const void *p1, const void *p2) noexcept
{
	return _tcscmp(reinterpret_cast<const SED2KFileType*>(p1)->pszExt, reinterpret_cast<const SED2KFileType*>(p2)->pszExt);
}

EED2KFileType GetED2KFileTypeID(LPCTSTR pszFileName)
{
	LPCTSTR pszExt = _tcsrchr(pszFileName, _T('.'));
	if (pszExt == NULL)
		return ED2KFT_ANY;
	CString strExt(pszExt);

	SED2KFileType ft;
	ft.pszExt = strExt.MakeLower();
	ft.iFileType = ED2KFT_ANY;
	const SED2KFileType *pFound = (SED2KFileType*)bsearch(&ft, g_aED2KFileTypes, _countof(g_aED2KFileTypes), sizeof g_aED2KFileTypes[0], CompareE2DKFileType);
	if (pFound != NULL)
		return pFound->iFileType;
	return ED2KFT_ANY;
}

// Returns the ed2k file type string ID which is to be used for publishing+searching
LPCTSTR GetED2KFileTypeSearchTerm(EED2KFileType iFileID)
{
	switch (iFileID) {
	case ED2KFT_AUDIO:
		return _T(ED2KFTSTR_AUDIO);
	case ED2KFT_VIDEO:
		return _T(ED2KFTSTR_VIDEO);
	case ED2KFT_IMAGE:
		return _T(ED2KFTSTR_IMAGE);
	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	// NOTE: If this gets changed, the function 'GetED2KFileTypeSearchID' also needs to get updated!
	case ED2KFT_ARCHIVE:
	case ED2KFT_CDIMAGE:
	case ED2KFT_PROGRAM:
		return _T(ED2KFTSTR_PROGRAM);
	case ED2KFT_DOCUMENT:
		return _T(ED2KFTSTR_DOCUMENT);
	case ED2KFT_EMULECOLLECTION:
		return _T(ED2KFTSTR_EMULECOLLECTION);
	}
	return NULL;
}

// Returns the ed2k file type integer ID which is to be used for publishing+searching
EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID)
{
	switch (iFileID) {
	case ED2KFT_AUDIO:
	case ED2KFT_VIDEO:
	case ED2KFT_IMAGE:
	case ED2KFT_PROGRAM:
	case ED2KFT_DOCUMENT:
		return iFileID;
	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	// NOTE: If this gets changed, the function 'GetED2KFileTypeSearchTerm' also needs to get updated!
	case ED2KFT_ARCHIVE:
	case ED2KFT_CDIMAGE:
		return ED2KFT_PROGRAM;
	}
	return ED2KFT_ANY;
}

// Returns a file type which is used eMule internally only, examining the extension of the given filename
CString GetFileTypeByName(LPCTSTR pszFileName)
{
	LPCTSTR p;
	switch (GetED2KFileTypeID(pszFileName)) {
	case ED2KFT_AUDIO:
		p = _T(ED2KFTSTR_AUDIO);
		break;
	case ED2KFT_VIDEO:
		p = _T(ED2KFTSTR_VIDEO);
		break;
	case ED2KFT_IMAGE:
		p = _T(ED2KFTSTR_IMAGE);
		break;
	case ED2KFT_DOCUMENT:
		p = _T(ED2KFTSTR_DOCUMENT);
		break;
	case ED2KFT_PROGRAM:
		p = _T(ED2KFTSTR_PROGRAM);
		break;
	case ED2KFT_ARCHIVE:
		p = _T(ED2KFTSTR_ARCHIVE);
		break;
	case ED2KFT_CDIMAGE:
		p = _T(ED2KFTSTR_CDIMAGE);
		break;
	case ED2KFT_EMULECOLLECTION:
		p = _T(ED2KFTSTR_EMULECOLLECTION);
		break;
	default:
		p = _T("");
	}
	return CString(p);
}

// Returns a file type which is used eMule internally only (GUI)
CString GetFileTypeDisplayStrFromED2KFileType(LPCTSTR pszED2KFileType)
{
	UINT uid = 0;
	ASSERT(pszED2KFileType != NULL);
	if (pszED2KFileType != NULL) {
		if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_AUDIO)) == 0)
			uid = IDS_SEARCH_AUDIO;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_VIDEO)) == 0)
			uid = IDS_SEARCH_VIDEO;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_IMAGE)) == 0)
			uid = IDS_SEARCH_PICS;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_DOCUMENT)) == 0)
			uid = IDS_SEARCH_DOC;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_PROGRAM)) == 0)
			uid = IDS_SEARCH_PRG;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_ARCHIVE)) == 0)
			uid = IDS_SEARCH_ARC;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_CDIMAGE)) == 0)
			uid = IDS_SEARCH_CDIMG;
		else if (_tcscmp(pszED2KFileType, _T(ED2KFTSTR_EMULECOLLECTION)) == 0)
			uid = IDS_SEARCH_EMULECOLLECTION;
	}
	return uid ? GetResString(uid) : CString();
}

class CED2KFileTypes
{
public:
	CED2KFileTypes()
	{
		qsort(g_aED2KFileTypes, _countof(g_aED2KFileTypes), sizeof g_aED2KFileTypes[0], CompareE2DKFileType);
#ifdef _DEBUG
		// check for duplicate entries
		LPCTSTR pszLast = g_aED2KFileTypes[0].pszExt;
		for (int i = 1; i < _countof(g_aED2KFileTypes); ++i) {
			ASSERT(_tcscmp(pszLast, g_aED2KFileTypes[i].pszExt) != 0);
			pszLast = g_aED2KFileTypes[i].pszExt;
		}
#endif
	}
};
CED2KFileTypes theED2KFileTypes; // get the list sorted *before* any code is accessing it

const BYTE* FindPattern(const BYTE *pucBuff, int iBuffSize, const BYTE *pucPattern, int iPatternSize)
{
	for (int iSearchRange = iBuffSize - iPatternSize; iSearchRange >= 0; --iSearchRange) {
		if (memcmp(pucBuff, pucPattern, iPatternSize) == 0)
			return pucBuff;
		++pucBuff;
	}
	return NULL;
}
/* use StrStrI() instead
const TCHAR* stristr(const TCHAR *str1, const TCHAR *str2)
{
	if (!*str2)
		return str1;

	for (LPCTSTR cp = str1; *cp; ++cp) {
		LPCTSTR s1 = cp;
		LPCTSTR s2 = str2;

		while (*s1 && *s2 && _totlower(*s1) == _totlower(*s2)) {
			++s1;
			++s2;
		}

		if (!*s2)
			return cp;
	}

	return NULL;
}
*/
CString GetNextString(const CString &rstr, LPCTSTR pszTokens, int &riStart)
{
	CString strResult;
	if (pszTokens != NULL && *pszTokens && riStart >= 0) {
		int iToken = rstr.Find(pszTokens, riStart);
		if (iToken >= 0) {
			int iLen = iToken - riStart;
			if (iLen >= 0) {
				strResult = rstr.Mid(riStart, iLen);
				riStart += iLen + 1;
			}
		} else {
			strResult = rstr.Mid(riStart);
			riStart = -1;
		}
	}
	return strResult;
}

CString GetNextString(const CString &rstr, TCHAR chToken, int &riStart)
{
	int iStart = riStart;
	int iLen;
	if (chToken != _T('\0') && iStart >= 0) {
		int iToken = rstr.Find(chToken, iStart);
		if (iToken >= 0) {
			iLen = iToken - iStart;
			if (iLen >= 0)
				riStart += iLen + 1;
		} else {
			iLen = rstr.GetLength() - iStart;
			riStart = -1;
		}
	} else
		iLen = 0;
	return rstr.Mid(iStart, iLen);
}

int GetSystemErrorString(DWORD dwError, CString &rstrError)
{
	// FormatMessage language flags:
	//
	// - MFC uses: MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT)
	//				SUBLANG_SYS_DEFAULT = 0x02 (system default)
	//
	// - SDK uses: MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)
	//				SUBLANG_DEFAULT		= 0x01 (user default)
	//
	//
	// Found in "winnt.h"
	// ------------------
	//  Language IDs.
	//
	//  The following two combinations of primary language ID and
	//  sublanguage ID have special semantics:
	//
	//    Primary Language ID   Sublanguage ID      Result
	//    -------------------   ---------------     ------------------------
	//    LANG_NEUTRAL          SUBLANG_NEUTRAL     Language neutral
	//    LANG_NEUTRAL          SUBLANG_DEFAULT     User default language
	//    LANG_NEUTRAL          SUBLANG_SYS_DEFAULT System default language
	//
	// *** SDK notes also:
	// If you pass in zero, 'FormatMessage' looks for a message for LANGIDs in
	// the following order:
	//
	//	1) Language neutral
	//	2) Thread LANGID, based on the thread's locale value
	//  3) User default LANGID, based on the user's default locale value
	//	4) System default LANGID, based on the system default locale value
	//	5) US English
	LPTSTR pszSysMsg = NULL;
	DWORD dwLength = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
		(LPTSTR)&pszSysMsg, 0, NULL);
	if (dwLength != 0 && pszSysMsg != NULL) {
		if (dwLength >= 2 && pszSysMsg[dwLength - 2] == _T('\r'))
			pszSysMsg[dwLength - 2] = _T('\0');
		rstrError = pszSysMsg;
		rstrError.Replace(_T("\r\n"), _T(" ")); // some messages contain CRLF within the message!?
	} else
		rstrError.Empty();

	if (pszSysMsg)
		LocalFree(pszSysMsg);

	return rstrError.GetLength();
}

int GetModuleErrorString(DWORD dwError, CString &rstrError, LPCTSTR pszModule)
{
	LPTSTR pszSysMsg = NULL;
	DWORD dwLength = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		GetModuleHandle(pszModule), dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT),
		(LPTSTR)&pszSysMsg, 0, NULL);
	if (dwLength != 0 && pszSysMsg != NULL) {
		if (dwLength >= 2 && pszSysMsg[dwLength - 2] == _T('\r'))
			pszSysMsg[dwLength - 2] = _T('\0');
		rstrError = pszSysMsg;
		rstrError.Replace(_T("\r\n"), _T(" ")); // some messages contain CRLF within the message!?
	} else
		rstrError.Empty();

	if (pszSysMsg)
		LocalFree(pszSysMsg);

	return rstrError.GetLength();
}

int GetErrorMessage(DWORD dwError, CString &rstrErrorMsg, DWORD dwFlags)
{
	int iMsgLen = GetSystemErrorString(dwError, rstrErrorMsg);
	if (iMsgLen == 0)
		rstrErrorMsg.Format(((long)dwError >= 0) ? _T("Error %u") : _T("Error 0x%08x"), dwError);
	else if (dwFlags & 1) {
		CString sMsg;
		sMsg.Format(((long)dwError >= 0) ? _T("Error %u: ") : _T("Error 0x%08x: "), dwError);
		rstrErrorMsg.Insert(0, sMsg);
	}
	return rstrErrorMsg.GetLength();
}

CString GetErrorMessage(DWORD dwError, DWORD dwFlags)
{
	CString strError;
	GetErrorMessage(dwError, strError, dwFlags);
	return strError;
}

BOOL GetExceptionMessage(const CException &ex, LPTSTR lpszErrorMsg, UINT nMaxError)
{
	BOOL ret = ex.GetErrorMessage(lpszErrorMsg, nMaxError);
	if (lpszErrorMsg)
		lpszErrorMsg[(ret && nMaxError) ? nMaxError - 1 : 0] = 0; //terminate string
	return ret;
}

LPCTSTR GetShellExecuteErrMsg(DWORD dwShellExecError)
{
	/* The error codes returned from 'ShellExecute' consist of the 'old' WinExec
	 * error codes and some additional error codes.
	 *
	 * Error					Code							WinExec
	 * ----------------------------------------------------------------------------
	 * 0						0	"Out of memory"				Yes
	 * ERROR_FILE_NOT_FOUND		2	(== SE_ERR_FNF)				Yes
	 * SE_ERR_FNF				2	(== ERROR_FILE_NOT_FOUND)	Yes
	 * ERROR_PATH_NOT_FOUND		3	(== SE_ERR_PNF)				Yes
	 * SE_ERR_PNF				3	(== ERROR_PATH_NOT_FOUND)	Yes
	 * SE_ERR_ACCESSDENIED		5								Yes
	 * SE_ERR_OOM				8								Yes
	 * ERROR_BAD_FORMAT			11								Yes
	 * SE_ERR_SHARE				26								No
	 * SE_ERR_ASSOCINCOMPLETE	27								No
	 * SE_ERR_DDETIMEOUT		28								No
	 * SE_ERR_DDEFAIL			29								No
	 * SE_ERR_DDEBUSY			30								No
	 * SE_ERR_NOASSOC			31								No
	 * SE_ERR_DLLNOTFOUND		32								No
	 *
	 * Using "FormatMessage(FORMAT_MESSAGE_FROM_HMODULE, <shell32.dll>" does *not* work!
	 */
	static const struct _tagERRMSG
	{
		UINT	uId;
		LPCTSTR	pszMsg;
	} s_aszWinExecErrMsg[] =
	{
	  { 0,
		_T("The operating system is out of memory or resources.")},
	  { 1,
		_T("Call to MS-DOS Int 21 Function 4B00h was invalid.")},
	  {/* 2*/SE_ERR_FNF,
		_T("The specified file was not found.")},
	  {/* 3*/SE_ERR_PNF,
		_T("The specified path was not found.")},
	  { 4,
		_T("Too many files were open.")},
	  {/* 5*/SE_ERR_ACCESSDENIED,
		_T("The operating system denied access to the specified file.")},
	  { 6,
		_T("Library requires separate data segments for each task.")},
	  { 7,
		_T("There were MS-DOS Memory block problems.")},
	  {/* 8*/SE_ERR_OOM,
		_T("There was insufficient memory to start the application.")},
	  { 9,
		_T("There were MS-DOS Memory block problems.")},
	  {10,
		_T("Windows version was incorrect.")},
	  {11,
		_T("Executable file was invalid. Either it was not a Windows application or there was an error in the .EXE image.")},
	  {12,
		_T("Application was designed for a different operating system.")},
	  {13,
		_T("Application was designed for MS-DOS 4.0.")},
	  {14,
		_T("Type of executable file was unknown.")},
	  {15,
		_T("Attempt was made to load a real-mode application (developed for an earlier version of Windows).")},
	  {16,
		_T("Attempt was made to load a second instance of an executable file containing multiple data segments that were not marked read-only.")},
	  {17,
		_T("Attempt was made in large-frame EMS mode to load a second instance of an application that links to certain non-shareable DLLs that are already in use.")},
	  {18,
		_T("Attempt was made in real mode to load an application marked for use in protected mode only.")},
	  {19,
		_T("Attempt was made to load a compressed executable file. The file must be decompressed before it can be loaded.")},
	  {20,
		_T("Dynamic-link library (DLL) file was invalid. One of the DLLs required to run this application was corrupt.")},
	  {21,
		_T("Application requires Microsoft Windows 32-bit extensions.")},
	  /*22-31	 RESERVED FOR FUTURE USE. NOT RETURNED BY VERSION 3.0.*/
	  {24,
		_T("Command line too long.")}, // 30.03.99 []: Seen under WinNT 4.0/Win98 for a very long command line!

	  /*non-WinExec error codes*/
	  {/*26*/SE_ERR_SHARE,
		_T("A sharing violation occurred.")},
	  {/*27*/SE_ERR_ASSOCINCOMPLETE,
		_T("The file name association is incomplete or invalid.")},
	  {/*28*/SE_ERR_DDETIMEOUT,
		_T("The DDE transaction could not be completed because the request timed out.")},
	  {/*29*/SE_ERR_DDEFAIL,
		_T("The DDE transaction failed.")},
	  {/*30*/SE_ERR_DDEBUSY,
		_T("The DDE transaction could not be completed because other DDE transactions were being processed.")},
	  {/*31*/SE_ERR_NOASSOC,
		_T("There is no application associated with the given file name extension.")},
	  {/*32*/SE_ERR_DLLNOTFOUND,
		_T("The specified dynamic-link library was not found.")}
	};

	if (dwShellExecError > 32)
		return _T("");	// No error

	// Search error message
	for (UINT i = 0; i < _countof(s_aszWinExecErrMsg); ++i)
		if (s_aszWinExecErrMsg[i].uId == dwShellExecError)
			return s_aszWinExecErrMsg[i].pszMsg;

	return _T("Unknown error.");
}

int GetDesktopColorDepth()
{
	HDC hdcScreen = ::GetDC(HWND_DESKTOP);
	int iColorBits = ::GetDeviceCaps(hdcScreen, BITSPIXEL) * ::GetDeviceCaps(hdcScreen, PLANES);
	::ReleaseDC(HWND_DESKTOP, hdcScreen);
	return iColorBits;
}

int GetAppImageListColorFlag()
{
	int iColorBits = GetDesktopColorDepth();
	int iIlcFlag;
	if (iColorBits >= 32)
		iIlcFlag = ILC_COLOR32;
	else if (iColorBits >= 24)
		iIlcFlag = ILC_COLOR24;
	else if (iColorBits >= 16)
		iIlcFlag = ILC_COLOR16;
	else if (iColorBits >= 8)
		iIlcFlag = ILC_COLOR8;
	else if (iColorBits >= 4)
		iIlcFlag = ILC_COLOR4;
	else
		iIlcFlag = ILC_COLOR;
	return iIlcFlag;
}

CString DbgGetHexDump(const uint8 *data, UINT size)
{
	CString buffer;
	buffer.Format(_T("Size=%u, Data=["), size);
	UINT i;
	for (i = 0; i < size && i < 50; ++i)
		buffer.AppendFormat(i ? _T(" %02x") : _T("%02x"), data[i]);

	buffer += (i < size) ? _T("]") : _T("..]");
	return buffer;
}

void DbgSetThreadName(LPCSTR szThreadName, ...)
{
#ifdef DEBUG

#ifndef MS_VC_EXCEPTION
#define MS_VC_EXCEPTION 0x406d1388

#pragma pack(push, 8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType;		// must be 0x1000
		LPCSTR szName;		// pointer to name (in same addr space)
		DWORD dwThreadID;	// thread ID (-1 caller thread)
		DWORD dwFlags;		// reserved for future use, must be zero
	} THREADNAME_INFO;
#endif
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		va_list args;
		va_start(args, szThreadName);
		int lenBuf = 0;
		char *buffer = NULL;
		int lenResult;
		do { // the VS debugger truncates the string to 31 characters anyway!
			lenBuf += 128;
			delete[] buffer;
			buffer = new char[lenBuf];
			lenResult = _vsnprintf(buffer, lenBuf, szThreadName, args);
		} while (lenResult == -1);
		va_end(args);
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = buffer;
		info.dwThreadID = _UI32_MAX;
		info.dwFlags = 0;
		__try {
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		} __except (EXCEPTION_CONTINUE_EXECUTION) {
		}
		delete[] buffer;
	} __except (EXCEPTION_CONTINUE_EXECUTION) {
	}
#pragma warning(pop)
#else
	UNREFERENCED_PARAMETER(szThreadName);
#endif
}

CString RemoveFileExtension(const CString &rstrFilePath)
{
	int iDot = rstrFilePath.ReverseFind(_T('.'));
	return (iDot < 0) ? rstrFilePath : rstrFilePath.Left(iDot);
}

int CompareDirectory(const CString &rstrDir1, const CString &rstrDir2)
{
	// use case insensitive compare as a starter
	if (rstrDir1.CompareNoCase(rstrDir2) == 0)
		return 0;
	// if one of the paths ends with a '\' the paths may still be equal from the file system's POV
	CString strDir1(rstrDir1);
	CString strDir2(rstrDir2);
	unslosh(strDir1);
	unslosh(strDir2);
	return strDir1.CompareNoCase(strDir2);	// compare again
}

bool IsGoodIP(uint32 nIP, bool forceCheck)
{
	// always filter following IP's
	// -------------------------------------------
	// 0.0.0.0							invalid
	// 127.0.0.0 - 127.255.255.255		Loopback
	// 224.0.0.0 - 239.255.255.255		Multicast
	// 240.0.0.0 - 255.255.255.254		Reserved for Future Use
	// 255.255.255.255					invalid

	if (nIP == 0 || (uint8)nIP == 127 || (uint8)nIP >= 224) {
#ifdef _DEBUG
		return ((uint8)nIP == 127 && thePrefs.GetAllowLocalHostIP());
#else
		return false;
#endif
	}

	return (!thePrefs.FilterLANIPs() && !forceCheck) || !IsLANIP(nIP);
}

bool IsLANIP(uint32 nIP)
{
	// LAN IP's
	// -------------------------------------------
	//	0.*								"This" Network
	//	10.0.0.0 - 10.255.255.255		Class A
	//	172.16.0.0 - 172.31.255.255		Class B
	//	192.168.0.0 - 192.168.255.255	Class C

	uint8 nFirst = (uint8)nIP;
	uint8 nSecond = (uint8)(nIP >> 8);

	if (nFirst == 192 && nSecond == 168) // check this 1st, because those LANs IPs are most common
		return true;

	if (nFirst == 172 && nSecond >= 16 && nSecond <= 31)
		return true;

	return nFirst == 0 || nFirst == 10;
}

bool IsGoodIPPort(uint32 nIP, uint16 nPort)
{
	return IsGoodIP(nIP) && nPort != 0;
}

CString GetFormatedUInt(ULONG ulVal)
{
	TCHAR szVal[12];
	_ultot(ulVal, szVal, 10);

	static NUMBERFMT nf;
	if (nf.Grouping == 0) {
		nf.NumDigits = 0;
		nf.LeadingZero = 0;
		nf.Grouping = 3;
		// we are hardcoding the following two format chars intentionally because the C-RTL also has the decimal sep hardcoded to '.'
		nf.lpDecimalSep = _T(".");
		nf.lpThousandSep = _T(",");
		nf.NegativeOrder = 0;
	}
	CString strVal;
	const int iBuffSize = _countof(szVal) * 2;
	int iResult = GetNumberFormat(LOCALE_SYSTEM_DEFAULT, 0, szVal, &nf, strVal.GetBuffer(iBuffSize), iBuffSize);
	strVal.ReleaseBuffer();
	return iResult ? strVal : CString(szVal);
}

CString GetFormatedUInt64(ULONGLONG ullVal)
{
	TCHAR szVal[24];
	_ui64tot(ullVal, szVal, 10);

	static NUMBERFMT nf;
	if (nf.Grouping == 0) {
		nf.NumDigits = 0;
		nf.LeadingZero = 0;
		nf.Grouping = 3;
		// we are hardcoding the following two format chars intentionally because the C-RTL also has the decimal sep hardcoded to '.'
		nf.lpDecimalSep = _T(".");
		nf.lpThousandSep = _T(",");
		nf.NegativeOrder = 0;
	}
	const int iBuffSize = _countof(szVal) * 2;
	CString strVal;
	int iResult = GetNumberFormat(LOCALE_SYSTEM_DEFAULT, 0, szVal, &nf, strVal.GetBuffer(iBuffSize), iBuffSize);
	strVal.ReleaseBuffer();
	return iResult ? strVal : CString(szVal);
}

void Debug(LPCTSTR pszFmtMsg, ...)
{
	va_list pArgs;
	va_start(pArgs, pszFmtMsg);
	CString strBuff;
#ifdef _DEBUG
	time_t tNow = time(NULL);
	int iTimeLen = (int)_tcsftime(strBuff.GetBuffer(40), 40, _T("%H:%M:%S "), localtime(&tNow));
	strBuff.ReleaseBuffer(iTimeLen);
#endif
	strBuff.AppendFormatV(pszFmtMsg, pArgs);

	// get around a bug in the debug device which is not capable of dumping long strings
	for (int i = 0; i < strBuff.GetLength(); i += 1024)
		OutputDebugString(strBuff.Mid(i, 1024));

	va_end(pArgs);
}

void DebugHexDump(const uint8 *data, UINT lenData)
{
	int lenLine = 16;
	CStringA line, single;
	for (UINT pos = 0; pos < lenData; pos += lenLine) {
		line.Format("%08X ", pos);
		lenLine = (int)min((lenData - pos), 16u);
		for (int i = 0; i < lenLine; ++i) {
			single.Format(" %02X", data[pos + i]);
			line += single;
			if (i == 7)
				line += ' ';
		}
		line += CStringA(' ', 60 - line.GetLength());
		for (int i = 0; i < lenLine; ++i) {
			char c = (char)data[pos + i];
			line += (c > 31 && c < 127) ? c : '.';
		}
		Debug(_T("%hs\n"), (LPCSTR)line);
	}
}

void DebugHexDump(CFile &file)
{
	int iSize = (int)(file.GetLength() - file.GetPosition());
	if (iSize > 0) {
		uint8 *data = NULL;
		try {
			data = new uint8[iSize];
			file.Read(data, iSize);
			DebugHexDump(data, iSize);
		} catch (CFileException *e) {
			TRACE("*** DebugHexDump(CFile&); CFileException\n");
			e->Delete();
		} catch (CMemoryException *e) {
			TRACE("*** DebugHexDump(CFile&); CMemoryException\n");
			e->Delete();
		}
		delete[] data;
	}
}

LPCTSTR DbgGetFileNameFromID(const uchar *hash)
{
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID(hash);
	if (reqfile != NULL)
		return reqfile->GetFileName();

	CPartFile *partfile = theApp.downloadqueue->GetFileByID(hash);
	if (partfile != NULL)
		return partfile->GetFileName();

	CKnownFile *knownfile = theApp.knownfiles->FindKnownFileByID(hash);
	return knownfile ? knownfile->GetFileName() : NULL;
}

CString DbgGetFileInfo(const uchar *hash)
{
	CString strInfo;
	if (hash != NULL) {
		LPCTSTR pszName = DbgGetFileNameFromID(hash);
		strInfo.Format(_T("File=%s"), (pszName != NULL ? pszName : (LPCTSTR)md4str(hash)));
	}
	return strInfo;
}

CString DbgGetFileStatus(UINT nPartCount, CSafeMemFile *data)
{
	CString strFileStatus;
	if (nPartCount == 0)
		strFileStatus = _T("Complete");
	else {
		CString strPartStatus;
		UINT nAvailableParts = 0;
		UINT nPart = 0;
		while (nPart < nPartCount) {
			uint8 ucPartMask;
			try {
				ucPartMask = data->ReadUInt8();
			} catch (CFileException *ex) {
				ex->Delete();
				strPartStatus = _T("*PacketException*");
				break;
			}
			for (int i = 0; i < 8; ++i) {
				bool bPartAvailable = (((ucPartMask >> i) & 1) != 0);
				nAvailableParts += static_cast<int>(bPartAvailable);
				strPartStatus += bPartAvailable ? _T('#') : _T('.');
				if (++nPart >= nPartCount)
					break;
			}
		}
		strFileStatus.Format(_T("Parts=%u  Avail=%u  %s"), nPartCount, nAvailableParts, (LPCTSTR)strPartStatus);
	}
	return strFileStatus;
}

CString DbgGetBlockInfo(const Requested_Block_Struct *block)
{
	return DbgGetBlockInfo(block->StartOffset, block->EndOffset);
}

CString DbgGetBlockInfo(uint64 StartOffset, uint64 EndOffset)
{
	CString strInfo;
	strInfo.Format(_T("%I64u-%I64u (%I64u bytes)"), StartOffset, EndOffset, EndOffset - StartOffset + 1);
	uint64 so = StartOffset / PARTSIZE;
	uint64 eo = EndOffset / PARTSIZE;
	if (so == eo)
		strInfo.AppendFormat(_T(", Part %I64u"), so);
	else
		strInfo.AppendFormat(_T(", Part %I64u-%I64u(**)"), so, eo);

	so = StartOffset / EMBLOCKSIZE;
	eo = EndOffset / EMBLOCKSIZE;
	if (so == eo)
		strInfo.AppendFormat(_T(", Block %I64u"), so);
	else
		strInfo.AppendFormat(_T(", Block %I64u-%I64u%s"), so, eo, (eo - so > 1) ? _T("(**)") : _T(""));

	return strInfo;
}

CString DbgGetBlockFileInfo(const Requested_Block_Struct *block, const CPartFile *partfile)
{
	CString strInfo(DbgGetBlockInfo(block));
	strInfo.AppendFormat(_T("; %s"), (LPCTSTR)DbgGetFileInfo(partfile ? partfile->GetFileHash() : NULL));
	return strInfo;
}

LPCTSTR DbgGetHashTypeString(const uchar *hash)
{
	if (hash[5] == 13 && hash[14] == 110)
		return _T("Old eMule");
	if (hash[5] == 14 && hash[14] == 111)
		return _T("eMule");
	if (hash[5] == 'M' && hash[14] == 'L')
		return _T("MLdonkey");
	return _T("Unknown");
}

CString DbgGetClientID(uint32 nClientID)
{
	if (::IsLowID(nClientID)) {
		CString strClientID;
		strClientID.Format(_T("LowID=%u"), nClientID);
		return strClientID;
	}
	return ipstr(nClientID);
}

#define _STRVAL(o)	{_T(#o), o}

CString DbgGetDonkeyClientTCPOpcode(UINT opcode)
{
	static const struct
	{
		LPCTSTR pszOpcode;
		UINT uOpcode;
	} _aOpcodes[] =
	{
		_STRVAL(OP_HELLO),
		_STRVAL(OP_SENDINGPART),
		_STRVAL(OP_REQUESTPARTS),
		_STRVAL(OP_FILEREQANSNOFIL),
		_STRVAL(OP_END_OF_DOWNLOAD),
		_STRVAL(OP_ASKSHAREDFILES),
		_STRVAL(OP_ASKSHAREDFILESANSWER),
		_STRVAL(OP_HELLOANSWER),
		_STRVAL(OP_CHANGE_CLIENT_ID),
		_STRVAL(OP_MESSAGE),
		_STRVAL(OP_SETREQFILEID),
		_STRVAL(OP_FILESTATUS),
		_STRVAL(OP_HASHSETREQUEST),
		_STRVAL(OP_HASHSETANSWER),
		_STRVAL(OP_STARTUPLOADREQ),
		_STRVAL(OP_ACCEPTUPLOADREQ),
		_STRVAL(OP_CANCELTRANSFER),
		_STRVAL(OP_OUTOFPARTREQS),
		_STRVAL(OP_REQUESTFILENAME),
		_STRVAL(OP_REQFILENAMEANSWER),
		_STRVAL(OP_CHANGE_SLOT),
		_STRVAL(OP_QUEUERANK),
		_STRVAL(OP_ASKSHAREDDIRS),
		_STRVAL(OP_ASKSHAREDFILESDIR),
		_STRVAL(OP_ASKSHAREDDIRSANS),
		_STRVAL(OP_ASKSHAREDFILESDIRANS),
		_STRVAL(OP_ASKSHAREDDENIEDANS)
	};

	for (unsigned i = 0; i < _countof(_aOpcodes); ++i)
		if (_aOpcodes[i].uOpcode == opcode)
			return _aOpcodes[i].pszOpcode;

	CString strOpcode;
	strOpcode.Format(_T("0x%02x"), opcode);
	return strOpcode;
}

CString DbgGetMuleClientTCPOpcode(UINT opcode)
{
	static const struct
	{
		LPCTSTR pszOpcode;
		UINT uOpcode;
	} _aOpcodes[] =
	{
		_STRVAL(OP_EMULEINFO),
		_STRVAL(OP_EMULEINFOANSWER),
		_STRVAL(OP_COMPRESSEDPART),
		_STRVAL(OP_QUEUERANKING),
		_STRVAL(OP_FILEDESC),
		_STRVAL(OP_REQUESTSOURCES),
		_STRVAL(OP_ANSWERSOURCES),
		_STRVAL(OP_REQUESTSOURCES2),
		_STRVAL(OP_ANSWERSOURCES2),
		_STRVAL(OP_PUBLICKEY),
		_STRVAL(OP_SIGNATURE),
		_STRVAL(OP_SECIDENTSTATE),
		_STRVAL(OP_REQUESTPREVIEW),
		_STRVAL(OP_PREVIEWANSWER),
		_STRVAL(OP_MULTIPACKET),
		_STRVAL(OP_MULTIPACKETANSWER),
		_STRVAL(OP_PEERCACHE_QUERY),
		_STRVAL(OP_PEERCACHE_ANSWER),
		_STRVAL(OP_PEERCACHE_ACK),
		_STRVAL(OP_PUBLICIP_ANSWER),
		_STRVAL(OP_PUBLICIP_REQ),
		_STRVAL(OP_PORTTEST),
		_STRVAL(OP_CALLBACK),
		_STRVAL(OP_BUDDYPING),
		_STRVAL(OP_BUDDYPONG),
		_STRVAL(OP_REASKCALLBACKTCP),
		_STRVAL(OP_AICHANSWER),
		_STRVAL(OP_AICHREQUEST),
		_STRVAL(OP_AICHFILEHASHANS),
		_STRVAL(OP_AICHFILEHASHREQ),
		_STRVAL(OP_COMPRESSEDPART_I64),
		_STRVAL(OP_SENDINGPART_I64),
		_STRVAL(OP_REQUESTPARTS_I64),
		_STRVAL(OP_MULTIPACKET_EXT),
		_STRVAL(OP_CHATCAPTCHAREQ),
		_STRVAL(OP_CHATCAPTCHARES),
		_STRVAL(OP_FWCHECKUDPREQ),
		_STRVAL(OP_KAD_FWTCPCHECK_ACK)
	};

	for (unsigned i = 0; i < _countof(_aOpcodes); ++i)
		if (_aOpcodes[i].uOpcode == opcode)
			return _aOpcodes[i].pszOpcode;

	CString strOpcode;
	strOpcode.Format(_T("0x%02x"), opcode);
	return strOpcode;
}

#undef _STRVAL

CString DbgGetClientTCPPacket(UINT protocol, UINT opcode, UINT size)
{
	CString str;
	if (protocol == OP_EDONKEYPROT)
		str.Format(_T("protocol=eDonkey  opcode=%s  size=%u"), (LPCTSTR)DbgGetDonkeyClientTCPOpcode(opcode), size);
	else if (protocol == OP_PACKEDPROT)
		str.Format(_T("protocol=Packed  opcode=%s  size=%u"), (LPCTSTR)DbgGetMuleClientTCPOpcode(opcode), size);
	else if (protocol == OP_EMULEPROT)
		str.Format(_T("protocol=eMule  opcode=%s  size=%u"), (LPCTSTR)DbgGetMuleClientTCPOpcode(opcode), size);
	else
		str.Format(_T("protocol=0x%02x  opcode=0x%02x  size=%u"), protocol, opcode, size);
	return str;
}

CString DbgGetClientTCPOpcode(UINT protocol, UINT opcode)
{
	switch (protocol) {
	case OP_EDONKEYPROT:
		return DbgGetDonkeyClientTCPOpcode(opcode);
	case OP_PACKEDPROT:
	case OP_EMULEPROT:
		return DbgGetMuleClientTCPOpcode(opcode);
	}
	CString str;
	str.Format(_T("protocol=0x%02x  opcode=0x%02x"), protocol, opcode);
	return str;
}

void DebugRecv(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet, uint32 nIP)
{
	// 111.222.333.444 = 15 chars
	if (client) {
		if (packet != NULL)
			Debug(_T("%-24hs from %s; %s\n"), pszMsg, (LPCTSTR)client->DbgGetClientInfo(true), (LPCTSTR)DbgGetFileInfo(packet));
		else
			Debug(_T("%-24hs from %s\n"), pszMsg, (LPCTSTR)client->DbgGetClientInfo(true));
	} else {
		if (nIP != 0)
			if (packet != NULL)
				Debug(_T("%-24hs from %-15s; %s\n"), pszMsg, (LPCTSTR)ipstr(nIP), (LPCTSTR)DbgGetFileInfo(packet));
			else
				Debug(_T("%-24hs from %-15s\n"), pszMsg, (LPCTSTR)ipstr(nIP));
		else
			if (packet != NULL)
				Debug(_T("%-24hs; %s\n"), pszMsg, (LPCTSTR)DbgGetFileInfo(packet));
			else
				Debug(_T("%-24hs\n"), pszMsg);
	}
}

void DebugSend(LPCSTR pszMsg, const CUpDownClient *client, const uchar *packet)
{
	if (client != NULL && packet != NULL)
		Debug(_T(">>> %-20hs to   %s; %s\n"), pszMsg, (LPCTSTR)client->DbgGetClientInfo(true), (LPCTSTR)DbgGetFileInfo(packet));
	else if (client != NULL && packet == NULL)
		Debug(_T(">>> %-20hs to   %s\n"), pszMsg, (LPCTSTR)client->DbgGetClientInfo(true));
	else if (client == NULL && packet != NULL)
		Debug(_T(">>> %-20hs; %s\n"), pszMsg, (LPCTSTR)DbgGetFileInfo(packet));
	else
		Debug(_T(">>> %-20hs\n"), pszMsg);
}

void DebugSend(LPCSTR pszOpcode, uint32 ip, uint16 port)
{
	TCHAR szIPPort[22];
	_stprintf(szIPPort, _T("%s:%u"), (LPCTSTR)ipstr(htonl(ip)), port);
	Debug(_T(">>> %-20hs to   %-21s\n"), pszOpcode, szIPPort);
}

void DebugSendF(LPCSTR pszOpcode, uint32 ip, uint16 port, LPCTSTR pszMsg, ...)
{
	va_list args;
	va_start(args, pszMsg);
	TCHAR szIPPort[22];
	_stprintf(szIPPort, _T("%s:%u"), (LPCTSTR)ipstr(htonl(ip)), port);
	CString str;
	str.Format(_T(">>> %-20hs to   %-21s; "), pszOpcode, szIPPort);
	str.AppendFormatV(pszMsg, args);
	va_end(args);
	Debug(_T("%s\n"), (LPCTSTR)str);
}

void DebugRecv(LPCSTR pszOpcode, uint32 ip, uint16 port)
{
	TCHAR szIPPort[22];
	_stprintf(szIPPort, _T("%s:%u"), (LPCTSTR)ipstr(htonl(ip)), port);
	Debug(_T("%-24hs from %-21s\n"), pszOpcode, szIPPort);
}

void DebugHttpHeaders(const CStringAArray &astrHeaders)
{
	for (int i = 0; i < astrHeaders.GetCount(); ++i) {
		const CStringA &rstrHdr = astrHeaders[i];
		Debug(_T("<%hs\n"), (LPCSTR)rstrHdr);
	}
}

ULONGLONG GetDiskFileSize(LPCTSTR pszFilePath)
{
	static BOOL _bInitialized = FALSE;
	static DWORD(WINAPI *_pfnGetCompressedFileSize)(LPCTSTR, LPDWORD) = NULL;

	if (!_bInitialized) {
		_bInitialized = TRUE;
		(FARPROC&)_pfnGetCompressedFileSize = GetProcAddress(GetModuleHandle(_T("kernel32")), _TWINAPI("GetCompressedFileSize"));
	}

	// If the file is not compressed nor sparse, 'GetCompressedFileSize' returns the 'normal' file size.
	if (_pfnGetCompressedFileSize) {
		ULONGLONG ullCompFileSize;
		((LPDWORD)&ullCompFileSize)[0] = (*_pfnGetCompressedFileSize)(pszFilePath, &((LPDWORD)&ullCompFileSize)[1]);
		if (((LPDWORD)&ullCompFileSize)[0] != INVALID_FILE_SIZE || ::GetLastError() == NO_ERROR)
			return ullCompFileSize;
	}

	// If 'GetCompressedFileSize' failed or is not available, use the default function
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(pszFilePath, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	FindClose(hFind);

	return (ULONGLONG)fd.nFileSizeHigh << 32 | (ULONGLONG)fd.nFileSizeLow;
}

// Listview helper function
void GetPopupMenuPos(const CListCtrl &lv, CPoint &point)
{
	// If the context menu was not opened using the right mouse button,
	// but the keyboard (Shift+F10), get a useful position for the context menu.
	if (point.x == -1 && point.y == -1) {
		int iSel = lv.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
		if (iSel >= 0) {
			RECT rc;
			if (lv.GetItemRect(iSel, &rc, LVIR_BOUNDS)) {
				point.SetPoint(rc.left + lv.GetColumnWidth(0) / 2, (rc.top + rc.bottom) / 2);
				lv.ClientToScreen(&point);
			}
		} else {
			point.SetPoint(16, 32);
			lv.ClientToScreen(&point);
		}
	}
}

void GetPopupMenuPos(const CTreeCtrl &tv, CPoint &point)
{
	// If the context menu was not opened using the right mouse button,
	// but the keyboard (Shift+F10), get a useful position for the context menu.
	if (point.x == -1 && point.y == -1) {
		HTREEITEM hSel = tv.GetNextItem(TVI_ROOT, TVGN_CARET);
		if (hSel) {
			CRect rcItem;
			if (tv.GetItemRect(hSel, &rcItem, TRUE)) {
				point = rcItem.TopLeft();
				tv.ClientToScreen(&point);
			}
		} else {
			point.SetPoint(16, 32);
			tv.ClientToScreen(&point);
		}
	}
}

time_t safe_mktime(struct tm *ptm)
{
	return ptm ? mktime(ptm) : -1;
}

CString StripInvalidFilenameChars(const CString &strText)
{
	CString strDest;

	for (LPCTSTR pszSource = strText; *pszSource; ++pszSource)
		if (*pszSource >= _T('\x20') && !_tcschr(sBadFileNameChar, *pszSource))
			strDest += *pszSource;

	static LPCTSTR const apszReservedFilenames[] = {
		_T("NUL"), _T("CON"), _T("PRN"), _T("AUX"), _T("CLOCK$"),
		_T("COM1"),_T("COM2"),_T("COM3"),_T("COM4"),_T("COM5"),_T("COM6"),_T("COM7"),_T("COM8"),_T("COM9"),
		_T("LPT1"),_T("LPT2"),_T("LPT3"),_T("LPT4"),_T("LPT5"),_T("LPT6"),_T("LPT7"),_T("LPT8"),_T("LPT9")
	};
	for (unsigned i = 0; i < _countof(apszReservedFilenames); ++i) {
		int nPrefixLen = (uint32)_tcslen(apszReservedFilenames[i]);
		if (_tcsnicmp(strDest, apszReservedFilenames[i], nPrefixLen) == 0) {
			if (strDest.GetLength() == nPrefixLen) {
				// Filename is a reserved file name:
				// Append an underscore character
				strDest += _T('_');
				break;
			}
			if (strDest[nPrefixLen] == _T('.')) {
				// Filename starts with a reserved file name followed by a '.' character:
				// Replace that ',' character with an '_' character.
				strDest.SetAt(nPrefixLen, _T('_'));
				break;
			}
		}
	}

	return strDest;
}

bool operator==(const CCKey &k1, const CCKey &k2)
{
	return md4equ(k1.m_key, k2.m_key);
}

bool operator==(const CSKey &k1, const CSKey &k2)
{
	return md4equ(k1.m_key, k2.m_key);
}

CString ipstr(LPCTSTR pszAddress, uint16 nPort)
{
	CString strIPPort;
	strIPPort.Format(_T("%s:%u"), pszAddress, nPort);
	return strIPPort;
}

CString ipstr(uint32 nIP, uint16 nPort)
{
	return ipstr((LPCTSTR)ipstr(nIP), nPort);
}

CString ipstr(uint32 nIP)
{
	return CString(inet_ntoa(*(in_addr*)&nIP));
}

CStringA ipstrA(uint32 nIP)
{
	return CStringA(inet_ntoa(*(in_addr*)&nIP));
}

void ipstrA(CHAR *pszAddress, int iMaxAddress, uint32 nIP)
{
	const BYTE *pucIP = (BYTE*)&nIP;
	_snprintf(pszAddress, iMaxAddress, "%u.%u.%u.%u", pucIP[0], pucIP[1], pucIP[2], pucIP[3]);
}

bool IsDaylightSavingTimeActive(LONG &rlDaylightBias)
{
	TIME_ZONE_INFORMATION tzi;
	if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_DAYLIGHT)
		return false;
	rlDaylightBias = tzi.DaylightBias;
	return true;
}

class CVolumeInfo
{
public:
	static bool IsNTFSVolume(LPCTSTR pszVolume)
	{
		return (GetVolumeInfo(pszVolume) == _T("NTFS"));
	}

	static bool IsFATVolume(LPCTSTR pszVolume)
	{
		return (_tcsnicmp(GetVolumeInfo(pszVolume), _T("FAT"), 3) == 0);
	}

	static const CString& GetVolumeInfo(LPCTSTR pszVolume)
	{
		CString strVolumeId(pszVolume);
		ASSERT(strVolumeId.Right(1) == _T("\\"));
		strVolumeId.MakeLower();
		CMapStringToString::CPair *pPair = m_mapVolumeInfo.PLookup(strVolumeId);
		if (pPair != NULL)
			return pPair->value;

		// 'GetVolumeInformation' may cause a noticeable delay - depending on the type of volume
		// which is queried. As we are using that function for almost every file (for compensating
		// the NTFS file time issues), we need to cash this information.
		//
		// The cache gets cleared when the user manually hits the 'Reload' button in the 'Shared
		// Files' window and when Windows broadcasts a message about that a volume was mounted/unmounted.
		//
		DWORD dwMaximumComponentLength = 0;
		DWORD dwFileSystemFlags = 0;
		TCHAR szFileSystemNameBuffer[MAX_PATH + 1];
		if (!GetVolumeInformation(pszVolume, NULL, 0, NULL, &dwMaximumComponentLength, &dwFileSystemFlags, szFileSystemNameBuffer, _countof(szFileSystemNameBuffer)))
			szFileSystemNameBuffer[0] = _T('\0');
		return m_mapVolumeInfo[strVolumeId] = szFileSystemNameBuffer;
	}

	static void ClearCache(int iDrive)
	{
		if (iDrive == -1)
			m_mapVolumeInfo.RemoveAll();
		else {
			TCHAR szRoot[MAX_PATH];
			szRoot[0] = _T('\0');
			PathBuildRoot(szRoot, iDrive);
			if (szRoot[0] != _T('\0')) {
				CString strVolumeId(szRoot);
				ASSERT(strVolumeId.Right(1) == _T("\\"));
				strVolumeId.MakeLower();
				m_mapVolumeInfo.RemoveKey(strVolumeId);
			}
		}
	}

protected:
	static CMapStringToString m_mapVolumeInfo;
};

static CVolumeInfo g_VolumeInfo;
CMapStringToString CVolumeInfo::m_mapVolumeInfo;

bool IsNTFSVolume(LPCTSTR pszVolume)
{
	return g_VolumeInfo.IsNTFSVolume(pszVolume);
}

bool IsFATVolume(LPCTSTR pszVolume)
{
	return g_VolumeInfo.IsFATVolume(pszVolume);
}

void ClearVolumeInfoCache(int iDrive)
{
	g_VolumeInfo.ClearCache(iDrive);
}

bool IsFileOnNTFSVolume(LPCTSTR pszFilePath)
{
	CString strRootPath(pszFilePath);
	BOOL bResult = PathStripToRoot(strRootPath.GetBuffer());
	strRootPath.ReleaseBuffer();
	if (!bResult)
		return false;
	// Need to add a trailing backslash in case of a network share
	if (!strRootPath.IsEmpty() && strRootPath[strRootPath.GetLength() - 1] != _T('\\')) {
		PathAddBackslash(strRootPath.GetBuffer(strRootPath.GetLength() + 1));
		strRootPath.ReleaseBuffer();
	}
	return IsNTFSVolume(strRootPath);
}

bool IsFileOnFATVolume(LPCTSTR pszFilePath)
{
	CString strRootPath(pszFilePath);
	BOOL bResult = PathStripToRoot(strRootPath.GetBuffer());
	strRootPath.ReleaseBuffer();
	if (!bResult)
		return false;
	// Need to add a trailing backslash in case of a network share
	if (!strRootPath.IsEmpty() && strRootPath[strRootPath.GetLength() - 1] != _T('\\')) {
		PathAddBackslash(strRootPath.GetBuffer(strRootPath.GetLength() + 1));
		strRootPath.ReleaseBuffer();
	}
	return IsFATVolume(strRootPath);
}

bool IsAutoDaylightTimeSetActive()
{
	CRegKey key;
	if (key.Open(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation"), KEY_READ) == ERROR_SUCCESS) {
		DWORD dwDisableAutoDaylightTimeSet = 0;
		if (key.QueryDWORDValue(_T("DisableAutoDaylightTimeSet"), dwDisableAutoDaylightTimeSet) == ERROR_SUCCESS)
			if (dwDisableAutoDaylightTimeSet)
				return false;
	}
	return true; // default to 'Automatically adjust clock for daylight saving changes'
}

bool AdjustNTFSDaylightFileTime(time_t &ruFileDate, LPCTSTR pszFilePath)
{
	if (!thePrefs.GetAdjustNTFSDaylightFileTime())
		return false;
	if (ruFileDate == 0 || ruFileDate == (time_t)-1)
		return false;

	// See also KB 129574
	LONG lDaylightBias = 0;
	if (IsDaylightSavingTimeActive(lDaylightBias)) {
		if (IsAutoDaylightTimeSetActive()) {
			if (IsFileOnNTFSVolume(pszFilePath)) {
				ruFileDate += MIN2S(lDaylightBias);
				return true;
			}
		} else {
			// If 'Automatically adjust clock for daylight saving changes' is disabled and
			// if the file's date is within DST period, we get again a wrong file date.
			//
			// If 'Automatically adjust clock for daylight saving changes' is disabled,
			// Windows always reports 'Active DST'(!!) with a bias of '0' although there is no
			// DST specified. This means also, that there is no chance to determine if a date is
			// within any DST.

			// Following code might be correct, but because we don't have a DST and because we don't have a bias,
			// the code won't do anything useful.
			/*struct tm *ptm = localtime((time_t*)&ruFileDate);
			bool bFileDateInDST = (ptm && ptm->tm_isdst == 1);
			if (bFileDateInDST) {
				ruFileDate += lDaylightBias*60;
				return true;
			}*/
		}
	}

	return false;
}

__time64_t FileTimeToUnixTime(const FILETIME &ft)
{
	const ULONGLONG t = *(UNALIGNED64 ULONGLONG*)&ft;
	return t ? t / 10000000ull - 11644473600ull : 0;
}

int statUTC(LPCTSTR pname, struct _stat64 &ft)
{
	WIN32_FILE_ATTRIBUTE_DATA fa;
	if (GetFileAttributesEx(pname, GetFileExInfoStandard, &fa)) {
		memset(&ft, 0, sizeof ft);
		ft.st_atime = FileTimeToUnixTime(fa.ftLastAccessTime);
		ft.st_ctime = FileTimeToUnixTime(fa.ftCreationTime);
		ft.st_mtime = FileTimeToUnixTime(fa.ftLastWriteTime);
		ft.st_size = ((__int64)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
		return 0;
	}
	return -1;
}

int statUTC(int ifile, struct _stat64 &ft)
{
	BY_HANDLE_FILE_INFORMATION fi;
	if (GetFileInformationByHandle((HANDLE)_get_osfhandle(ifile), &fi)) {
		memset(&ft, 0, sizeof ft);
		ft.st_atime = FileTimeToUnixTime(fi.ftLastAccessTime);
		ft.st_ctime = FileTimeToUnixTime(fi.ftCreationTime);
		ft.st_mtime = FileTimeToUnixTime(fi.ftLastWriteTime);
		ft.st_size = ((__int64)fi.nFileSizeHigh << 32) | fi.nFileIndexLow;
		return 0;
	}
	return -1;
}

bool ExpandEnvironmentStrings(CString &rstrStrings)
{
	DWORD dwSize = ExpandEnvironmentStrings(rstrStrings, NULL, 0);
	if (dwSize == 0)
		return false;

	CString strExpanded;
	DWORD dwCount = ExpandEnvironmentStrings(rstrStrings, strExpanded.GetBuffer(dwSize - 1), dwSize);
	if (dwCount == 0 || dwCount != dwSize) {
		ASSERT(0);
		return false;
	}
	strExpanded.ReleaseBuffer(dwCount - 1);
	rstrStrings = strExpanded;
	return true;
}

uint16 GetRandomUInt16()
{
#if RAND_MAX == 0x7fff
	// get 2 random numbers
	UINT uRand0 = rand();
	UINT uRand1 = rand();

	// NOTE: if that assert fires, you have most likely called that function *without* calling 'srand' first.
	// NOTE: each spawned thread HAS to call 'srand' for itself to get real random numbers.
	ASSERT(!(uRand0 == 41 && uRand1 == 18467));

	return (uint16)(uRand0 | ((uRand1 >= RAND_MAX / 2) ? 0x8000 : 0x0000));
#else
#error "Implement it!"
#endif
}

uint32 GetRandomUInt32()
{
#if RAND_MAX == 0x7fff
	//return ((uint32)GetRandomUInt16() << 16) | (uint32)GetRandomUInt16();
	// this would give the receiver the following information:
	//	random number N
	//	random number N+1 is below or greater/equal than 0x8000
	//	random number N+2
	//	random number N+3 is below or greater/equal than 0x8000

	uint32 uRand0 = GetRandomUInt16();
	srand(::GetTickCount() | uRand0);
	uint32 uRand1 = GetRandomUInt16();
	return (uRand0 << 16) | uRand1;
#else
#error "Implement it!"
#endif
}

HWND ReplaceRichEditCtrl(CWnd *pwndRE, CWnd *pwndParent, CFont *pFont)
{
	HWND hwndNewRE = NULL;

	ASSERT(pwndRE);
	if (pwndRE) {
		TCHAR szClassName[MAX_PATH];
		if (GetClassName(pwndRE->m_hWnd, szClassName, _countof(szClassName)) && _tcsicmp(szClassName, _T("RichEdit20W")) == 0)
			return NULL;

		CRect rcWnd;
		pwndRE->GetWindowRect(rcWnd);

		DWORD dwStyle = pwndRE->GetStyle();
		dwStyle |= WS_VSCROLL | WS_HSCROLL;
		DWORD dwExStyle = pwndRE->GetExStyle();

		CString strText;
		pwndRE->GetWindowText(strText);

		LONG_PTR uCtrlID = ::GetWindowLongPtr(*pwndRE, GWLP_ID);

		pwndRE->DestroyWindow();

		pwndParent->ScreenToClient(&rcWnd);
		hwndNewRE = ::CreateWindowEx(dwExStyle, RICHEDIT_CLASS, strText, dwStyle, rcWnd.left, rcWnd.top, rcWnd.Width(), rcWnd.Height(), pwndParent->m_hWnd, (HMENU)uCtrlID, NULL, NULL);
		if (hwndNewRE && pFont && pFont->m_hObject)
			::SendMessage(hwndNewRE, WM_SETFONT, (WPARAM)pFont->m_hObject, 0);
	}
	return hwndNewRE;
}

void InstallSkin(LPCTSTR pszSkinPackage)
{
	if (thePrefs.GetMuleDirectory(EMULE_SKINDIR).IsEmpty() || _taccess(thePrefs.GetMuleDirectory(EMULE_SKINDIR), 0) != 0) {
		LocMessageBox(IDS_INSTALL_SKIN_NODIR, MB_ICONERROR, 0);
		return;
	}

	static TCHAR const _szSkinSuffix[] = _T(".") EMULSKIN_BASEEXT _T(".ini");
	UINT uid = 0;
	CString szExt(PathFindExtension(pszSkinPackage));
	szExt.MakeLower();

	if (szExt == _T(".zip")) {
		CZIPFile zip;
		if (zip.Open(pszSkinPackage)) {
			// Search the "*.eMuleSkin.ini" file.
			CZIPFile::File *zfIniFile = NULL;
			for (int i = zip.GetCount(); --i >= 0;) {
				CZIPFile::File *zf = zip.GetFile(i);
				if (zf && zf->m_sName.Right(_countof(_szSkinSuffix) - 1).CompareNoCase(_szSkinSuffix) == 0) {
					zfIniFile = zf;
					break;
				}
			}

			if (zfIniFile) {
				for (int i = 0; i < zip.GetCount(); ++i) {
					CZIPFile::File *zf = zip.GetFile(i);
					if (!zf
						|| zf->m_sName.IsEmpty()
						|| zf->m_sName[0] == _T('\\')
						|| zf->m_sName[0] == _T('/')
						|| zf->m_sName.Find(_T(':')) >= 0
						|| zf->m_sName.Find(_T("..\\")) >= 0
						|| zf->m_sName.Find(_T("../")) >= 0)
					{
						continue;
					}

					if (zf->m_sName[zf->m_sName.GetLength() - 1] == _T('/')) {
						CString strDstDirPath;
						strDstDirPath.Format(_T("%s\\%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_SKINDIR), (LPCTSTR)zf->m_sName.Left(zf->m_sName.GetLength() - 1));
						canonical(strDstDirPath);
						if (!::CreateDirectory(strDstDirPath, NULL)) {
							DWORD dwError = ::GetLastError();
							CString strError;
							strError.Format(GetResString(IDS_INSTALL_SKIN_DIR_ERROR), (LPCTSTR)strDstDirPath, (LPCTSTR)GetErrorMessage(dwError));
							AfxMessageBox(strError, MB_ICONERROR);
							break;
						}
					} else {
						CString strDstFilePath;
						strDstFilePath.Format(_T("%s\\%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_SKINDIR), (LPCTSTR)zf->m_sName);
						canonical(strDstFilePath);
						::SetLastError(0);
						if (!zf->Extract(strDstFilePath)) {
							DWORD dwError = ::GetLastError();
							CString strError;
							strError.Format(GetResString(IDS_INSTALL_SKIN_FILE_ERROR), (LPCTSTR)zf->m_sName, (LPCTSTR)strDstFilePath, (LPCTSTR)GetErrorMessage(dwError));
							AfxMessageBox(strError, MB_ICONERROR);
							break;
						}
					}
				}
			} else
				uid = IDS_INSTALL_SKIN_PKG_ERROR;
			zip.Close();
		} else
			uid = IDS_INSTALL_SKIN_PKG_ERROR;
	} else if (szExt == _T(".rar")) {
		CRARFile rar;
		if (rar.Open(pszSkinPackage)) {
			bool bError = false;
			bool bFoundSkinINIFile = false;
			CString strFileName;
			while (rar.GetNextFile(strFileName)) {
				if (strFileName.IsEmpty()
					|| strFileName[0] == _T('\\')
					|| strFileName[0] == _T('/')
					|| strFileName.Find(_T(':')) >= 0
					|| strFileName.Find(_T("..\\")) >= 0
					|| strFileName.Find(_T("../")) >= 0)
				{
					rar.Skip();
					continue;
				}

				if (!bFoundSkinINIFile && strFileName.Right(_countof(_szSkinSuffix) - 1).CompareNoCase(_szSkinSuffix) == 0)
					bFoundSkinINIFile = true;

				// No need to care about possible available sub-directories. UnRAR.DLL cares about that automatically.
				CString strDstFilePath;
				strDstFilePath.Format(_T("%s\\%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_SKINDIR), (LPCTSTR)strFileName);
				canonical(strDstFilePath);
				::SetLastError(0);
				if (!rar.Extract(strDstFilePath)) {
					DWORD dwError = ::GetLastError();
					CString strError;
					strError.Format(GetResString(IDS_INSTALL_SKIN_FILE_ERROR), (LPCTSTR)strFileName, (LPCTSTR)strDstFilePath, (LPCTSTR)GetErrorMessage(dwError));
					AfxMessageBox(strError, MB_ICONERROR);
					bError = true;
					break;
				}
			}

			if (!bError && !bFoundSkinINIFile)
				uid = IDS_INSTALL_SKIN_PKG_ERROR;

			rar.Close();
		} else {
			CString strError;
			strError.Format(_T("%s\r\n\r\nDownload latest version of UNRAR.DLL from http://www.rarlab.com and copy UNRAR.DLL into eMule installation folder."), (LPCTSTR)GetResString(IDS_INSTALL_SKIN_PKG_ERROR));
			AfxMessageBox(strError, MB_ICONERROR);
		}
	}
	if (uid)
		LocMessageBox(uid, MB_ICONERROR, 0);
}

void TriggerPortTest(uint16 tcp, uint16 udp)
{
	CString m_sTestURL;

	// do not alter the connection test, this is a manual test only.
	// If you want to change the behaviour, use your server!
	m_sTestURL.Format(PORTTESTURL, tcp, udp, thePrefs.GetLanguageID());

	// the port check will need to do an obfuscated callback too if obfuscation is requested,
	// hence we have to provide our userhash so it can create the key
	if (thePrefs.IsClientCryptLayerRequested())
		m_sTestURL.AppendFormat(_T("&obf=1&clienthash=%s"), (LPCTSTR)md4str(thePrefs.GetUserHash()));
	else
		m_sTestURL += _T("&obf=0");
	ShellOpenFile(m_sTestURL);
}

 //CSTR_LESS_THAN=1 CSTR_EQUAL=2 CSTR_GREATER_THAN=3
int CompareLocaleStringNoCaseA(LPCSTR psz1, LPCSTR psz2)
{
	// SDK says: The 'CompareString' function is optimized to run at the highest speed when 'dwCmpFlags' is set to 0
	// or NORM_IGNORECASE, and 'cchCount1' and 'cchCount2' have the value -1.
	int iResult = CompareStringA(GetThreadLocale(), NORM_IGNORECASE, psz1, -1, psz2, -1);
	return iResult ? iResult - CSTR_EQUAL : 0;
}

int CompareLocaleStringNoCaseW(LPCWSTR psz1, LPCWSTR psz2)
{
	// SDK says: The 'CompareString' function is optimized to run at the highest speed when 'dwCmpFlags' is set to 0
	// or NORM_IGNORECASE, and 'cchCount1' and 'cchCount2' have the value -1.
	int iResult = CompareStringW(GetThreadLocale(), NORM_IGNORECASE, psz1, -1, psz2, -1);
	return iResult ? iResult - CSTR_EQUAL : 0;
}

int CompareLocaleString(LPCTSTR psz1, LPCTSTR psz2)
{
	// SDK says: The 'CompareString' function is optimized to run at the highest speed when 'dwCmpFlags' is set to 0
	// or NORM_IGNORECASE, and 'cchCount1' and 'cchCount2' have the value -1.
	int iResult = CompareString(GetThreadLocale(), 0, psz1, -1, psz2, -1);
	return iResult ? iResult - CSTR_EQUAL : 0;
}

int __cdecl CompareCStringPtrLocaleStringNoCase(const void *p1, const void *p2)
{
	return CompareLocaleStringNoCase(*(CString*)p1, *(CString*)p2);
}

int __cdecl CompareCStringPtrLocaleString(const void *p1, const void *p2)
{
	return CompareLocaleString(*(CString*)p1, *(CString*)p2);
}

void Sort(CStringArray &astr, int(__cdecl *pfnCompare)(const void*, const void*))
{
	qsort(astr.GetData(), astr.GetCount(), sizeof(CString*), pfnCompare);
}

int __cdecl CompareCStringPtrPtrLocaleStringNoCase(const void *p1, const void *p2)
{
	return CompareLocaleStringNoCase(**(CString**)p1, **(CString**)p2);
}

int __cdecl CompareCStringPtrPtrLocaleString(const void *p1, const void *p2)
{
	return CompareLocaleString(**(CString**)p1, **(CString**)p2);
}

void Sort(CSimpleArray<const CString*> &apstr, int(__cdecl *pfnCompare)(const void*, const void*))
{
	qsort(apstr.GetData(), apstr.GetSize(), sizeof(CString*), pfnCompare);
}

// SLUGFILLER: heapsortCompletesrc
void HeapSort(CArray<uint16, uint16> &count, UINT first, UINT last)
{
	for (UINT r = first; !(r & (UINT)INT_MIN) && (r << 1) < last; ) {
		UINT r2 = (r << 1) + 1;
		if (r2 != last)
			r2 += static_cast<uint16>(count[r2] < count[r2 + 1]);
		if (count[r] >= count[r2])
			break;
		uint16 t = count[r2];
		count[r2] = count[r];
		count[r] = t;
		r = r2;
	}
}
// SLUGFILLER: heapsortCompletesrc

static LPCTSTR const pstrKeyName = _T("eMuleAutoStart");

void AddAutoStart()
{
#ifndef _DEBUG
	RemAutoStart();
	TCHAR sExeFilePath[MAX_PATH];
	DWORD dwModPathLen = ::GetModuleFileName(NULL, sExeFilePath, _countof(sExeFilePath));
	if (dwModPathLen == 0 || dwModPathLen == _countof(sExeFilePath))
		return;
	CString sFullExeCommand;
	sFullExeCommand.Format(_T("%s -AutoStart"), sExeFilePath);
	CRegKey mKey;
	mKey.Create(HKEY_CURRENT_USER
		, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")
		, REG_NONE
		, REG_OPTION_NON_VOLATILE
		, KEY_ALL_ACCESS
		, NULL
		, NULL);
	if (mKey != NULL) {
		mKey.SetStringValue(pstrKeyName, sFullExeCommand);
		mKey.Close();
	}
#endif
}

void RemAutoStart()
{
	CRegKey mKey;
	mKey.Create(HKEY_CURRENT_USER
		, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")
		, REG_NONE
		, REG_OPTION_NON_VOLATILE
		, KEY_ALL_ACCESS
		, NULL
		, NULL);
	if (mKey != NULL) {
		mKey.DeleteValue(pstrKeyName);
		mKey.Close();
	}
}

void SetAutoStart(bool bOn)
{
	if (bOn)
		AddAutoStart();
	else
		RemAutoStart();
}

int FontPointSizeToLogUnits(int nPointSize)
{
	HDC hDC = ::GetDC(HWND_DESKTOP);
	if (hDC) {
		POINT pt;
#if 0
		// This is the same math which is performed by "CFont::CreatePointFont",
		// which is flawed because it does not perform any rounding. But without
		// performing the correct rounding one can not get the correct LOGFONT-height
		// for an 8pt font!
		//
		// PointSize	Result
		// -------------------
		// 8*10			10.666 -> 10 (cut down and thus wrong result)
		pt.y = ::GetDeviceCaps(hDC, LOGPIXELSY) * nPointSize;
		pt.y /= 720;
#else
		// This math accounts for proper rounding and thus we will get the correct results.
		//
		// PointSize	Result
		// -------------------
		// 8*10			10.666 -> 11 (rounded up and thus correct result)
		pt.y = ::MulDiv(::GetDeviceCaps(hDC, LOGPIXELSY), nPointSize, 720);
#endif
		pt.x = 0;
		::DPtoLP(hDC, &pt, 1);
		POINT ptOrg = {0, 0};
		::DPtoLP(hDC, &ptOrg, 1);
		nPointSize = -abs(pt.y - ptOrg.y);
		::ReleaseDC(HWND_DESKTOP, hDC);
	}
	return nPointSize;
}

bool CreatePointFontIndirect(CFont &rFont, const LOGFONT *lpLogFont)
{
	LOGFONT logFont = *lpLogFont;
	logFont.lfHeight = FontPointSizeToLogUnits(logFont.lfHeight);
	return rFont.CreateFontIndirect(&logFont) != FALSE;
}

bool PointInClient(const CWnd &wnd, const CPoint &point)
{
	if (point.x != -1 || point.y != -1) {
		CRect rcClient;
		wnd.GetClientRect(rcClient);
		wnd.ClientToScreen(rcClient);
		return rcClient.PtInRect(point);
	}
	return true;
}

void SetTabTitle(UINT uid, CPropertyPage *pPage, CPropertySheet *pSheet)
{
	if (!pSheet) {
		if (!pPage->m_hWnd)
			return;
		pSheet = static_cast<CPropertySheet*>(pPage->GetParent());
	}
	int iPage = pSheet->GetPageIndex(pPage);
	if (iPage >= 0) {
		const CString &sTitle(GetResString(uid));
		TC_ITEM ti;
		ti.mask = TCIF_TEXT;
		ti.pszText = const_cast<LPTSTR>((LPCTSTR)sTitle);
		pSheet->GetTabControl()->SetItem(iPage, &ti);
	}
}

bool CreatePointFont(CFont &rFont, int nPointSize, LPCTSTR lpszFaceName)
{
	LOGFONT logFont = {};
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfHeight = nPointSize;
	_tcsncpy(logFont.lfFaceName, lpszFaceName, _countof(logFont.lfFaceName));
	return CreatePointFontIndirect(rFont, &logFont);
}

bool IsUnicodeFile(LPCTSTR pszFilePath)
{
	WORD wBOM;
	FILE *fp = _tfsopen(pszFilePath, _T("rb"), _SH_DENYWR);
	bool bResult = (fp != NULL) && fread(&wBOM, sizeof wBOM, 1, fp) == 1 && wBOM == 0xFEFFui16;
	if (fp != NULL)
		fclose(fp);
	return bResult;
}

bool RegularExpressionMatch(const CString &regexpr, const CString &teststring)
{
	std::basic_regex<TCHAR> reFN(regexpr);
#ifdef UNICODE
	std::wcmatch mcUrl;
#else
	std::cmatch mcUrl;
#endif
	return std::regex_match((LPCTSTR)teststring, mcUrl, reFN);
}

ULONGLONG GetModuleVersion(LPCTSTR pszFilePath)
{
	ULONGLONG ullVersion = 0;
	DWORD dwUnused;
	DWORD dwVerInfSize = GetFileVersionInfoSize(const_cast<LPTSTR>(pszFilePath), &dwUnused);
	if (dwVerInfSize != 0) {
		LPBYTE pucVerInf = (LPBYTE)malloc(dwVerInfSize);
		if (pucVerInf) {
			if (GetFileVersionInfo(const_cast<LPTSTR>(pszFilePath), 0, dwVerInfSize, pucVerInf)) {
				VS_FIXEDFILEINFO *pFileInf = NULL;
				UINT uLen = 0;
				if (VerQueryValue(pucVerInf, _T("\\"), (LPVOID*)&pFileInf, &uLen) && pFileInf && uLen) {
					ullVersion = MAKEDLLVERULL(HIWORD(pFileInf->dwFileVersionMS), LOWORD(pFileInf->dwFileVersionMS),
						HIWORD(pFileInf->dwFileVersionLS), LOWORD(pFileInf->dwFileVersionLS));
				}
			}
			free(pucVerInf);
		}
	}
	return ullVersion;
}

ULONGLONG GetModuleVersion(HMODULE hModule)
{
	TCHAR szFilePath[MAX_PATH];
	DWORD dwModPathLen = ::GetModuleFileName(hModule, szFilePath, _countof(szFilePath));
	if (dwModPathLen == 0 || dwModPathLen == _countof(szFilePath))
		return 0;
	return GetModuleVersion(szFilePath);
}

int GetPathDriveNumber(const CString &path)
{
	if (path.GetLength() < 3 || path[1] != _T(':') || path[2] != _T('\\'))
		return -1;

	return tolower(path[0]) - 'a'; //97, lower case 'a'
}

uint64 GetFreeTempSpace(INT_PTR tempdirindex)
{
	if (tempdirindex >= thePrefs.GetTempDirCount() || tempdirindex < -1) {
		ASSERT(0);
		return 0;
	}

	if (tempdirindex >= 0)
		return GetFreeDiskSpaceX(thePrefs.GetTempDir(tempdirindex));

	CArray<int> hist;
	uint64 sum = 0;
	for (INT_PTR i = 0; i < thePrefs.GetTempDirCount(); ++i) {
		int pdn = GetPathDriveNumber(thePrefs.GetTempDir(i));
		if (pdn >= 0)
			for (INT_PTR j = hist.GetCount(); --j >= 0;)
				if (hist[j] == pdn) {
					pdn = -1;
					break;
				}

		if (pdn >= 0) {
			sum += GetFreeDiskSpaceX(thePrefs.GetTempDir(i));
			hist.Add(pdn);
		}
	}

	return sum;
}

bool DoCollectionRegFix(bool checkOnly)
{
	TCHAR modbuffer[MAX_PATH];
	DWORD dwModPathLen = ::GetModuleFileName(NULL, modbuffer, _countof(modbuffer));
	if (dwModPathLen == 0 || dwModPathLen == _countof(modbuffer))
		return false;
	CString strCanonFileName(modbuffer);
	strCanonFileName.Replace(_T("%"), _T("%%"));
	CString regbuffer;
	regbuffer.Format(_T("\"%s\" \"%%1\""), (LPCTSTR)strCanonFileName);

	// first check if the registry keys are already set (either by installer in HKLM or by user in HKCU)
	bool bGlobalSet = false;
	CRegKey regkey;
	LONG result = regkey.Open(HKEY_CLASSES_ROOT, _T("eMule\\shell\\open\\command"), KEY_READ);
	if (result == ERROR_SUCCESS) {
		TCHAR rbuffer[MAX_PATH + 100];
		ULONG maxsize = _countof(rbuffer);
		regkey.QueryStringValue(NULL, rbuffer, &maxsize);
		rbuffer[_countof(rbuffer) - 1] = _T('\0');
		if (maxsize != 0 && _tcsicmp(rbuffer, regbuffer) == 0)
			bGlobalSet = true; // yup, globally we have an entry for this mule
		regkey.Close();
	}

	if (!bGlobalSet) {
		// we actually need to change the registry and write an entry for HKCU
		if (checkOnly)
			return true;
		HKEY hkeyCR = thePrefs.GetWindowsVersion() < _WINVER_2K_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
		if (regkey.Create(hkeyCR, _T("Software\\Classes\\eMule\\shell\\open\\command")) == ERROR_SUCCESS) {
			VERIFY(regkey.SetStringValue(NULL, regbuffer) == ERROR_SUCCESS);

			VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\eMule\\DefaultIcon")) == ERROR_SUCCESS);
			VERIFY(regkey.SetStringValue(NULL, CString(modbuffer) + _T(",1")) == ERROR_SUCCESS);

			VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\eMule")) == ERROR_SUCCESS);
			VERIFY(regkey.SetStringValue(NULL, _T("eMule Collection File")) == ERROR_SUCCESS);

			VERIFY(regkey.Open(hkeyCR, _T("Software\\Classes\\eMule\\shell\\open")) == ERROR_SUCCESS);
			regkey.RecurseDeleteKey(_T("ddexec"));
			regkey.RecurseDeleteKey(_T("ddeexec"));

			VERIFY(regkey.Create(hkeyCR, _T("Software\\Classes\\") COLLECTION_FILEEXTENSION) == ERROR_SUCCESS);
			VERIFY(regkey.SetStringValue(NULL, _T("eMule")) == ERROR_SUCCESS);
			VERIFY(regkey.Close() == ERROR_SUCCESS);
		} else
			ASSERT(0);
	}
	return false;
}

bool gotostring(CFile &file, const uchar *find, LONGLONG plen)
{
	LONGLONG j = 0;
	ULONGLONG len = file.GetLength();
	uchar temp;

	for (ULONGLONG i = file.GetPosition(); i < len && file.Read(&temp, 1) == 1; ++i) {
		if (temp == find[j])
			++j;
		else
			j = static_cast<LONGLONG>(temp == find[0]);
		if (j >= plen)
			return true;
	}
	return false;
}

static inline void swap_byte(uint8 *a, uint8 *b)
{
	uint8 bySwap = *a;
	*a = *b;
	*b = bySwap;
}

RC4_Key_Struct* RC4CreateKey(const uchar *pachKeyData, uint32 nLen, RC4_Key_Struct *key, bool bSkipDiscard)
{
	if (key == NULL)
		key = new RC4_Key_Struct;

	uint8 *pabyState = key->abyState;
	for (uint32 i = 0; i < sizeof key->abyState; ++i)
		pabyState[i] = (uint8)i;

	key->byX = 0;
	key->byY = 0;
	uint8 index1 = 0;
	uint8 index2 = 0;
	for (uint32 i = 0; i < sizeof key->abyState; ++i) {
		index2 += pachKeyData[index1] + pabyState[i];
		swap_byte(&pabyState[i], &pabyState[index2]);
		index1 = (uint8)((index1 + 1) % nLen);
	}
	if (!bSkipDiscard)
		RC4Crypt(NULL, 1024, key);
	return key;
}

void RC4Crypt(const uchar *pachIn, uchar *pachOut, uint32 nLen, RC4_Key_Struct *key)
{
	ASSERT(key != NULL && nLen > 0);
	if (key == NULL)
		return;

	uint8 byX = key->byX;
	uint8 byY = key->byY;
	uint8 *pabyState = key->abyState;
	uint8 byXorIndex;

	for (uint32 i = 0; i < nLen; ++i) {
		byY += pabyState[++byX];
		swap_byte(&pabyState[byX], &pabyState[byY]);
		byXorIndex = (pabyState[byX] + pabyState[byY]);

		if (pachIn != NULL)
			pachOut[i] = pachIn[i] ^ pabyState[byXorIndex];
	}
	key->byX = byX;
	key->byY = byY;
}

void RC4Crypt(uchar *pach, uint32 nLen, RC4_Key_Struct *key)
{
	RC4Crypt(pach, pach, nLen, key);
}

struct SFileExts
{
	EFileType ftype;
	LPCTSTR label;
	LPCTSTR extlist;
};

static const SFileExts s_fileexts[] =
{
	{ARCHIVE_ZIP,			_T("ZIP"),			_T("|ZIP|JAR|CBZ|") },
	{ARCHIVE_RAR,			_T("RAR"),			_T("|RAR|CBR|") },
	{ARCHIVE_ACE,			_T("ACE"),			_T("|ACE|") },
	{ARCHIVE_7Z,			_T("7Z"),			_T("|7Z|") },
	{AUDIO_MPEG,			_T("MPEG Audio"),	_T("|MP2|MP3|") },
	{IMAGE_ISO,				_T("ISO/NRG"),		_T("|ISO|NRG|") },
	{VIDEO_MPG,				_T("MPEG Video"),	_T("|MPG|MPEG|") },
	{VIDEO_AVI,				_T("AVI"),			_T("|AVI|DIVX|") },
	{VIDEO_MP4,				_T("MP4"),			_T("|MP4|MOV|QT|")},
	{VIDEO_MKV,				_T("MKV"),			_T("|MKV|")},
	{VIDEO_OGG,				_T("OGG"),			_T("|OGG|OGM|")},
	{WM,					_T("Microsoft Media Audio/Video"), _T("|ASF|WMV|WMA|") },
	{PIC_JPG,				_T("JPEG"),			_T("|JPG|JPEG|") },
	{PIC_PNG,				_T("PNG"),			_T("|PNG|") },
	{PIC_GIF,				_T("GIF"),			_T("|GIF|") },
	{DOCUMENT_PDF,			_T("PDF"),			_T("|PDF|") },
	{FILETYPE_EXECUTABLE,	_T("WIN/DOS EXE"),	_T("|EXE|COM|DLL|SYS|CPL|FON|OCX|SCR|VBX|") },
	{FILETYPE_UNKNOWN,		_T(""),				_T("") }
};

static const byte FILEHEADER_7Z_ID[] =	{ 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C };
static const byte FILEHEADER_ACE_ID[] = { 0x2A, 0x2A, 0x41, 0x43, 0x45, 0x2A, 0x2A };
static const byte FILEHEADER_AVI_ID[] = { 0x52, 0x49, 0x46, 0x46 };
static const byte FILEHEADER_EXE_ID[] = { 0x4d, 0x5a };
static const byte FILEHEADER_GIF_ID[] = { 0x47, 0x49, 0x46, 0x38 };
static const byte FILEHEADER_ISO_ID[] = { 0x01, 0x43, 0x44, 0x30, 0x30, 0x31 };
static const byte FILEHEADER_JPG_ID[] = { 0xff, 0xd8, 0xff };
static const byte FILEHEADER_MKV_ID[] = { 0x1A, 0x45, 0xDF, 0xA3 };
static const byte FILEHEADER_MP3_ID[] = { 0x49, 0x44, 0x33, 0x03 };
static const byte FILEHEADER_MP3_ID2[] = { 0xFE, 0xFB };
static const byte FILEHEADER_MP4_ID[] = { 0x66, 0x74, 0x79, 0x70 };	//Enig123::please note that the header starts at the position 4, not 0
static const byte FILEHEADER_MPG_ID[] = { 0x00, 0x00, 0x01, 0xba };
static const byte FILEHEADER_OGG_ID[] = { 0x4F, 0x67, 0x67, 0x53 };
static const byte FILEHEADER_PDF_ID[] = { 0x25, 0x50, 0x44, 0x46 };
static const byte FILEHEADER_PNG_ID[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static const byte FILEHEADER_RAR_ID[] = { 0x52, 0x61, 0x72, 0x21 };
static const byte FILEHEADER_WM_ID[] =	{ 0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c };
static const byte FILEHEADER_ZIP_ID[] = { 0x50, 0x4b, 0x03, 0x04 };
//the longest one is FILEHEADER_WM_ID
#define HEADERCHECKSIZE (sizeof FILEHEADER_WM_ID)

bool GetDRM(LPCTSTR pszFilePath)
{
	int fd = _topen(pszFilePath, O_RDONLY | O_BINARY);
	if (fd != -1) {
		BYTE aucBuff[16384];
		int iRead = _read(fd, aucBuff, sizeof aucBuff);
		_close(fd);

		if ((size_t)iRead > sizeof FILEHEADER_WM_ID && memcmp(aucBuff, FILEHEADER_WM_ID, sizeof FILEHEADER_WM_ID) == 0) {
			iRead -= sizeof FILEHEADER_WM_ID;
			if (iRead > 0) {
				static const WCHAR s_wszWrmHdr[] = L"<WRMHEADER";
				if (FindPattern(aucBuff + sizeof FILEHEADER_WM_ID, iRead, (BYTE*)s_wszWrmHdr, sizeof s_wszWrmHdr - sizeof s_wszWrmHdr[0]))
					return true;
			}
		}
	}
	return false;
}

EFileType GetFileTypeEx(CShareableFile *kfile, bool checkextention, bool checkfileheader, bool nocached)
{
	if (!nocached && kfile->GetVerifiedFileType() != FILETYPE_UNKNOWN)
		return kfile->GetVerifiedFileType();

	// check file header first
	EFileType res = FILETYPE_UNKNOWN;
	CPartFile *pfile = static_cast<CPartFile*>(kfile);

	bool test4iso = (!kfile->IsPartFile()
		|| ((uint64)pfile->GetFileSize() > 0x8000 + HEADERCHECKSIZE && pfile->IsComplete(0x8000, 0x8000 + HEADERCHECKSIZE, true)));
	if (checkfileheader && (!kfile->IsPartFile() || pfile->IsComplete(0, HEADERCHECKSIZE, true) || test4iso)) {
		try {
			CFile inFile;
			if (inFile.Open(kfile->GetFilePath(), CFile::modeRead | CFile::shareDenyNone)) {
				unsigned char headerbuf[HEADERCHECKSIZE];
				if (!kfile->IsPartFile() || pfile->IsComplete(0, HEADERCHECKSIZE, true)) {
					int read = inFile.Read(headerbuf, HEADERCHECKSIZE);
					if (read == HEADERCHECKSIZE) {
						if (memcmp(headerbuf, FILEHEADER_ZIP_ID, sizeof FILEHEADER_ZIP_ID) == 0)
							res = ARCHIVE_ZIP;
						else if (memcmp(headerbuf, FILEHEADER_RAR_ID, sizeof FILEHEADER_RAR_ID) == 0)
							res = ARCHIVE_RAR;
						else if (memcmp(headerbuf + 7, FILEHEADER_ACE_ID, sizeof FILEHEADER_ACE_ID) == 0)
							res = ARCHIVE_ACE;
						else if (memcmp(headerbuf, FILEHEADER_7Z_ID, sizeof FILEHEADER_7Z_ID) == 0)
							res = ARCHIVE_7Z;
						else if (memcmp(headerbuf, FILEHEADER_WM_ID, sizeof FILEHEADER_WM_ID) == 0)
							res = WM;
						else if (memcmp(headerbuf, FILEHEADER_AVI_ID, sizeof FILEHEADER_AVI_ID) == 0
							&& strncmp((char*)headerbuf + 8, "AVI", 3) == 0)
							res = VIDEO_AVI;
						else if (memcmp(headerbuf, FILEHEADER_MP3_ID, sizeof FILEHEADER_MP3_ID) == 0
							|| memcmp(headerbuf, FILEHEADER_MP3_ID2, sizeof FILEHEADER_MP3_ID2) == 0)
							res = AUDIO_MPEG;
						else if (memcmp(headerbuf, FILEHEADER_MPG_ID, sizeof FILEHEADER_MPG_ID) == 0)
							res = VIDEO_MPG;
						else if (memcmp(headerbuf + 4, FILEHEADER_MP4_ID, sizeof(FILEHEADER_MP4_ID)) == 0)
							res = VIDEO_MP4;
						else if (memcmp(headerbuf, FILEHEADER_MKV_ID, sizeof(FILEHEADER_MKV_ID)) == 0)
							res = VIDEO_MKV;
						else if (memcmp(headerbuf, FILEHEADER_OGG_ID, sizeof(FILEHEADER_OGG_ID)) == 0)
							res = VIDEO_OGG;
						else if (memcmp(headerbuf, FILEHEADER_PDF_ID, sizeof FILEHEADER_PDF_ID) == 0)
							res = DOCUMENT_PDF;
						else if (memcmp(headerbuf, FILEHEADER_PNG_ID, sizeof FILEHEADER_PNG_ID) == 0)
							res = PIC_PNG;
						else if (memcmp(headerbuf, FILEHEADER_JPG_ID, sizeof FILEHEADER_JPG_ID) == 0
								&& (headerbuf[3] == 0xe1 || headerbuf[3] == 0xe0))
							res = PIC_JPG;
						else if (memcmp(headerbuf, FILEHEADER_GIF_ID, sizeof FILEHEADER_GIF_ID) == 0
								&& headerbuf[5] == 0x61 && (headerbuf[4] == 0x37 || headerbuf[4] == 0x39))
							res = PIC_GIF;
						else if (memcmp(headerbuf, FILEHEADER_EXE_ID, sizeof FILEHEADER_EXE_ID) == 0) {
							// This could be a self extracting RAR archive. If the ED2K name of the file
							// has the RAR extension we treat this 'EXE' file like an 'unverified' RAR file.
							if (_tcsicmp(PathFindExtension(kfile->GetFileName()), _T(".rar")) == 0)
								res = FILETYPE_UNKNOWN;
							else
								res = FILETYPE_EXECUTABLE;
						} else if ((headerbuf[0] & 0xFF) == 0xFF && (headerbuf[1] & 0xE0) == 0xE0)
							res = AUDIO_MPEG;
					}
				}
				if (res == FILETYPE_UNKNOWN && test4iso) {
					inFile.Seek(0x8000, CFile::begin);
					int read = inFile.Read(headerbuf, HEADERCHECKSIZE);
					if (read == HEADERCHECKSIZE) {
						if (memcmp(headerbuf, FILEHEADER_ISO_ID, sizeof FILEHEADER_ISO_ID) == 0)
							res = IMAGE_ISO;
					}
				}
				inFile.Close();
			}
		} catch (...) {
			ASSERT(0);
			return FILETYPE_UNKNOWN;
		}

		if (res != FILETYPE_UNKNOWN) {
			kfile->SetVerifiedFileType(res);
			return res;
		}
	}

	if (!checkextention)
		return res;

	CString extLC;
	int posD = kfile->GetFileName().ReverseFind(_T('.'));
	if (posD >= 0)
		extLC = kfile->GetFileName().Mid(posD + 1).MakeUpper();

	for (const SFileExts *ext = s_fileexts; ext->ftype != FILETYPE_UNKNOWN; ++ext) {
		const CString &testext(ext->extlist);
		if (testext.Find(_T('|') + extLC + _T('|')) >= 0)
			return ext->ftype;
	}

	// rar multivolume old naming
	if (extLC.GetLength() == 3 && extLC[0] == _T('R') && _istdigit(extLC[1]) && _istdigit(extLC[2]))
		return ARCHIVE_RAR;
	return FILETYPE_UNKNOWN;
}

CString GetFileTypeName(EFileType ftype)
{
	for (const SFileExts *ext = s_fileexts; ext->ftype != FILETYPE_UNKNOWN; ++ext)
		if (ftype == ext->ftype)
			return ext->label;

	return CString(_T('?'));
}

bool ExtensionIs(LPCTSTR pszFilePath, LPCTSTR pszExt)
{
	return _tcsicmp(PathFindExtension(pszFilePath), pszExt) == 0;
}

// 1 - type matches acceptable extension
// 0 - extension not found
//-1 - extension was found, but types did not match
int IsExtensionTypeOf(EFileType ftype, const CString &ext)
{
	CString fext;
	fext.Format(_T("|%s|"), (LPCTSTR)ext);
	for (const SFileExts *pext = s_fileexts; pext->ftype != FILETYPE_UNKNOWN; ++pext) {
		const CString &testext(pext->extlist);
		if (testext.Find(fext) >= 0)
			return (ftype == pext->ftype) ? 1 : -1;
	}
	return 0;
}

uint32 LevenshteinDistance(const CString &str1, const CString &str2)
{
	size_t n1 = str1.GetLength();
	size_t n2 = str2.GetLength();

	uint32_t *vec = new uint32_t[n1 + 1];
	for (uint32_t i = 0; i <= n1; ++i)
		vec[i] = i;

	uint32_t d_del = 0;
	for (uint32_t j = 0; j < n2;) {
		d_del = j;
		const TCHAR c2 = str2[j];
		vec[0] = ++j;
		for (uint32_t i = 0; i < n1;) {
			uint32_t d_ins = vec[i];
			uint32_t d_sub = d_del + static_cast<uint32_t>(str1[i] != c2);
			d_del = vec[++i];
			vec[i] = min(min(d_del, d_ins) + 1, d_sub);
		}
	}
	d_del = vec[n1];
	delete[] vec;
	return d_del;
}


// Wrapper for _tmakepath which ensures that the outputbuffer does not exceed MAX_PATH
// using a smaller buffer without checking the sizes prior calling this function is not safe
// If the resulting path would be bigger than MAX_PATH-1, it will be empty and return false (similar to PathCombine)
bool _tmakepathlimit(LPTSTR path, LPCTSTR drive, LPCTSTR dir, LPCTSTR fname, LPCTSTR ext)
{
	if (path == NULL) {
		ASSERT(0);
		return false;
	}

	TCHAR tchBuffer[_MAX_DRIVE + _MAX_DIR + _MAX_FNAME + _MAX_EXT + 8];
	_tmakepath(tchBuffer, drive, dir, fname, ext);

	size_t sLen = _tcslen(tchBuffer);
	if (sLen >= MAX_PATH) {
		path[0] = _T('\0');
		ASSERT(0);
		return false;
	}
	_tcscpy(path, tchBuffer);
	return true;
}

uint8 GetMyConnectOptions(bool bEncryption, bool bCallback)
{
	// Connect options Tag
	// 4 Reserved (!)
	// 1 Direct Callback
	// 1 CryptLayer Required
	// 1 CryptLayer Requested
	// 1 CryptLayer Supported
	const uint8 uSupportsCryptLayer = static_cast<uint8>(thePrefs.IsClientCryptLayerSupported() && bEncryption);
	const uint8 uRequestsCryptLayer = static_cast<uint8>(thePrefs.IsClientCryptLayerRequested() && bEncryption);
	const uint8 uRequiresCryptLayer = static_cast<uint8>(thePrefs.IsClientCryptLayerRequired() && bEncryption);
	// direct callback is only possible if connected to kad, TCP firewalled and verified UDP open (for example on a full cone NAT)
	const uint8 uDirectUDPCallback = static_cast<uint8>(bCallback && theApp.IsFirewalled() && Kademlia::CKademlia::IsRunning() && !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) && Kademlia::CUDPFirewallTester::IsVerified());

	const uint8 byCryptOptions = (uDirectUDPCallback << 3) | (uRequiresCryptLayer << 2) | (uRequestsCryptLayer << 1) | (uSupportsCryptLayer << 0);
	return byCryptOptions;
}

bool AddIconGrayscaledToImageList(CImageList &rList, HICON hIcon)
{
	// Use to create grey-scaled alpha using icons on WinXP and lower
	// Only works with edited CxImage lib, not 6.0 standard
	bool bResult = false;
	ICONINFO iinfo;
	if (::GetIconInfo(hIcon, &iinfo)) {
		CxImage cxGray;
		if (cxGray.CreateFromHBITMAP(iinfo.hbmColor)) {
			cxGray.GrayScale();
			HBITMAP hGrayBmp = cxGray.MakeBitmap(NULL, true);
			bResult = rList.Add(CBitmap::FromHandle(hGrayBmp), CBitmap::FromHandle(iinfo.hbmMask)) != -1;
			::DeleteObject(hGrayBmp);
		}
		::DeleteObject(iinfo.hbmColor);
		::DeleteObject(iinfo.hbmMask);
	}
	return bResult;
}