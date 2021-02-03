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
#include "StringConversion.h"
#include <atlenc.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


int utf8towc(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize)
{
	LPWSTR pwc0 = pwc;

	while (uUtf8Size && uWideCharSize) {
		BYTE ucChar = *pcUtf8++;
		if (ucChar < 0x80) {
			--uUtf8Size;
			--uWideCharSize;
			*pwc++ = ucChar;
		} else {
			if ((ucChar & 0xC0) != 0xC0)
				return -1; // Invalid UTF-8 string.
			BYTE ucMask = 0xE0;
			UINT uExpectedBytes = 1;
			while ((ucChar & ucMask) == ucMask) {
				ucMask |= ucMask >> 1;
				if (++uExpectedBytes > 3)
					return -1; // Invalid UTF-8 string.
			}

			if (uUtf8Size <= uExpectedBytes)
				return -1; // Invalid UTF-8 string.

			UINT uProcessedBytes = 1 + uExpectedBytes;
			UINT uWideChar = (UINT)(ucChar & ~ucMask);
			if (uExpectedBytes == 1) {
				if ((uWideChar & 0x1E) == 0)
					return -1; // Invalid UTF-8 string.
			} else {
				if (uWideChar == 0 && (*pcUtf8 & 0x3F & (ucMask << 1)) == 0)
					return -1; // Invalid UTF-8 string.

				if (uExpectedBytes == 2) {
					//if (uWideChar == 0x0D && ((BYTE)*pcUtf8 & 0x20))
					//    return -1;
				} else if (uExpectedBytes == 3) {
					if (uWideChar > 4)
						return -1; // Invalid UTF-8 string.
					if (uWideChar == 4 && *pcUtf8 & 0x30)
						return -1; // Invalid UTF-8 string.
				}
			}

			if (uWideCharSize < (UINT)(uExpectedBytes > 2) + 1)
				break; // buffer full

			while (uExpectedBytes--) {
				ucChar = *pcUtf8++;
				if ((ucChar & 0xC0) != 0x80)
					return -1; // Invalid UTF-8 string.
				uWideChar <<= 6;
				uWideChar |= (ucChar & 0x3F);
			}
			uUtf8Size -= uProcessedBytes;

			if (uWideChar < 0x10000) {
				--uWideCharSize;
				*pwc++ = (WCHAR)uWideChar;
			} else {
				uWideCharSize -= 2;
				uWideChar -= 0x10000;
				*pwc++ = (WCHAR)(0xD800 | (uWideChar >> 10));
				*pwc++ = (WCHAR)(0xDC00 | (uWideChar & 0x03FF));
			}
		}
	}

	return (int)(pwc - pwc0);
}

int ByteStreamToWideChar(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize)
{
	int iWideChars = utf8towc(pcUtf8, uUtf8Size, pwc, uWideCharSize);
	if (iWideChars < 0) {
		LPWSTR pwc0 = pwc;
		while (uUtf8Size && uWideCharSize) {
			if ((*pwc++ = *pcUtf8++) == L'\0')
				break;
			--uUtf8Size;
			--uWideCharSize;
		}
		iWideChars = (int)(pwc - pwc0);
	}
	return iWideChars;
}

/*void CreateBOMUTF8String(const CStringW &rwstr, CStringA &rstrUTF8)
{
	int iChars = AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), NULL, 0);
	int iRawChars = 3 + iChars;
	LPSTR pszUTF8 = rstrUTF8.GetBuffer(iRawChars);
	*pszUTF8++ = 0xEFU;
	*pszUTF8++ = 0xBBU;
	*pszUTF8++ = 0xBFU;
	AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), pszUTF8, iRawChars);
	rstrUTF8.ReleaseBuffer(iRawChars);
}*/

CStringA wc2utf8(const CStringW &rwstr)
{
	CStringA strUTF8;
	int iChars = AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), NULL, 0);
	if (iChars > 0) {
		LPSTR pszUTF8 = strUTF8.GetBuffer(iChars);
		AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), pszUTF8, iChars);
		strUTF8.ReleaseBuffer(iChars);
	}
	return strUTF8;
}

CString OptUtf8ToStr(const CStringA &rastr)
{
	CStringW wstr;
	int iMaxWideStrLen = rastr.GetLength();
	LPWSTR pwsz = wstr.GetBuffer(iMaxWideStrLen);
	int iWideChars = utf8towc(rastr, rastr.GetLength(), pwsz, iMaxWideStrLen);
	if (iWideChars <= 0) {
		// invalid UTF-8 string...
		wstr.ReleaseBuffer(0);
		wstr = rastr;				// convert with the local codepage
	} else
		wstr.ReleaseBuffer(iWideChars);
	return wstr;					// just return the string
}

CString OptUtf8ToStr(LPCSTR psz, int iLen)
{
	CStringW wstr;
	int iMaxWideStrLen = iLen;
	LPWSTR pwsz = wstr.GetBuffer(iMaxWideStrLen);
	int iWideChars = utf8towc(psz, iLen, pwsz, iMaxWideStrLen);
	if (iWideChars <= 0) {
		// invalid UTF-8 string...
		wstr.ReleaseBuffer(0);
		wstr = CString(psz, iLen);	// convert with the local codepage
	} else
		wstr.ReleaseBuffer(iWideChars);
	return wstr;					// just return the string
}

CString OptUtf8ToStr(const CStringW &rwstr)
{
	CStringA astr;
	for (int i = 0; i < rwstr.GetLength(); ++i) {
		if (rwstr[i] >= 0x100)
			// this is no UTF-8 string (it's already a Unicode string)...
			return rwstr;			// just return the string

		astr += (CHAR)rwstr[i];
	}
	return OptUtf8ToStr(astr);
}

CStringA StrToUtf8(const CString &rstr)
{
	return wc2utf8(rstr);
}

CString EncodeUrlUtf8(const CString &rstr)
{
	CString url;
	CStringA utf8(StrToUtf8(rstr));
	for (int i = 0; i < utf8.GetLength(); ++i) {
		// NOTE: The purpose of that function is to encode non-ASCII characters only for being used within
		// an ED2K URL. An ED2K URL is not conforming to any RFC, thus any unsafe URI characters are kept
		// as they are. The space character is though special and gets encoded as well.
		if ((BYTE)utf8[i] == '%' || (BYTE)utf8[i] == ' ' || (BYTE)utf8[i] >= 0x7F)
			url.AppendFormat(_T("%%%02X"), (BYTE)utf8[i]);
		else
			url += utf8[i];
	}
	return url;
}

CStringW DecodeDoubleEncodedUtf8(LPCWSTR pszFileName)
{
	int nChars = (int)wcslen(pszFileName);

	// Check if all characters are valid for UTF-8 value range
	for (int i = 0; i < nChars; ++i)
		if (pszFileName[i] > (WCHAR)0xFFU)
			return pszFileName; // string is already using Unicode character value range; return the original

	// Transform Unicode string to UTF-8 byte sequence
	CStringA strA;
	LPSTR pszA = strA.GetBuffer(nChars);
	for (int i = 0; i < nChars; ++i)
		pszA[i] = (CHAR)pszFileName[i];
	strA.ReleaseBuffer(nChars);

	// Decode the string with UTF-8
	CStringW strW;
	LPWSTR pszW = strW.GetBuffer(nChars);
	int iNewChars = utf8towc(strA, nChars, pszW, nChars);
	if (iNewChars < 0) {
		strW.ReleaseBuffer(0);
		return pszFileName;		// conversion error (not a valid UTF-8 string); return the original
	}
	strW.ReleaseBuffer(iNewChars);

	return strW;
}