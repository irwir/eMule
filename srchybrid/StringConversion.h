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
#pragma once

#include <atlenc.h>

inline bool IsValidEd2kString(const CString &str)
{
	// NOTE: The '?' is the character which is returned by windows if user entered
	// a Unicode string into an edit control when application runs in ANSI mode.
	// The '?' is also invalid in search expressions and filenames.
	return str.Find(_T('?')) < 0;
}

inline bool NeedUTF8String(LPCWSTR pwsz)
{
	// This function is used for determining whether it is valid to convert
	// a Unicode string into an MBCS string (e.g. for local storage in *.met
	// files) without losing any characters - or in other words whether a
	// string conversion from Unicode->MBCS->Unicode will lead to the same
	// result again. With certain code pages (e.g. 1251 - Cyrillic) some
	// characters from within the range 0x80-0xFF are converted to ASCII
	// characters! Thus to evaluate whether a string can safely get stored with
	// MBCS without losing any characters due to the conversion, the valid
	// character range must be 0x00-0x7F.
	while (*pwsz != L'\0' && *pwsz <= 0x007f) // VS2008: ATL_ASCII(0x007f) is no longer defined
		++pwsz;
	return *pwsz != L'\0';
}

/*#define ED2KCODEPAGE	28591 // ISO 8859-1 Latin I

void CreateBOMUTF8String(const CStringW &rwstrUnicode, CStringA &rstrUTF8);

inline void OptCreateED2KUTF8String(bool bOptUTF8, const CStringW &rwstr, CStringA &rstrUTF8)
{
	if (bOptUTF8 && NeedUTF8String(rwstr))
		CreateBOMUTF8String(rwstr, rstrUTF8);
	else {
		// backward compatibility: use local codepage
		UINT cp = bOptUTF8 ? ED2KCODEPAGE : _AtlGetConversionACP();
		int iSize = WideCharToMultiByte(cp, 0, rwstr, -1, NULL, 0, NULL, NULL);
		if (iSize >= 1) {
			int iChars = iSize - 1;
			LPSTR pszUTF8 = rstrUTF8.GetBuffer(iChars);
			WideCharToMultiByte(cp, 0, rwstr, -1, pszUTF8, iSize, NULL, NULL);
			rstrUTF8.ReleaseBuffer(iChars);
		}
	}
}*/

CStringA wc2utf8(const CStringW &rwstr);
CString OptUtf8ToStr(const CStringA &rastr);
CString OptUtf8ToStr(LPCSTR psz, int iLen);
CString OptUtf8ToStr(const CStringW &rwstr);
CStringA StrToUtf8(const CString &rstr);
CString EncodeUrlUtf8(const CString &rstr);
int utf8towc(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize);
int ByteStreamToWideChar(LPCSTR pcUtf8, UINT uUtf8Size, LPWSTR pwc, UINT uWideCharSize);
CStringW DecodeDoubleEncodedUtf8(LPCWSTR pszFileName);

#define	SHORT_ED2K_STR			256
#define	SHORT_RAW_ED2K_MB_STR	(SHORT_ED2K_STR*2)
#define	SHORT_RAW_ED2K_UTF8_STR	(SHORT_ED2K_STR*4)


///////////////////////////////////////////////////////////////////////////////
// TUnicodeToUTF8

template< int t_nBufferLength = SHORT_ED2K_STR * 4 >
class TUnicodeToUTF8
{
public:
	explicit TUnicodeToUTF8(const CStringW &rwstr)
	{
		int iBuffSize;
		int iMaxEncodedStrSize = rwstr.GetLength() * 4;
		if (iMaxEncodedStrSize > t_nBufferLength) {
			iBuffSize = iMaxEncodedStrSize;
			m_psz = new char[iBuffSize];
		} else {
			iBuffSize = _countof(m_acBuff);
			m_psz = m_acBuff;
		}

		m_iChars = AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), m_psz, iBuffSize);
		ASSERT(m_iChars > 0 || rwstr.IsEmpty());
	}

	explicit TUnicodeToUTF8(LPCWSTR pwsz, int iLength = -1)
	{
		if (iLength == -1)
			iLength = (int)wcslen(pwsz);
		int iBuffSize;
		int iMaxEncodedStrSize = iLength * 4;
		if (iMaxEncodedStrSize > t_nBufferLength) {
			iBuffSize = iMaxEncodedStrSize;
			m_psz = new char[iBuffSize];
		} else {
			iBuffSize = _countof(m_acBuff);
			m_psz = m_acBuff;
		}

		m_iChars = AtlUnicodeToUTF8(pwsz, iLength, m_psz, iBuffSize);
		ASSERT(m_iChars > 0 || iLength == 0);
	}

	~TUnicodeToUTF8()
	{
		if (m_psz != m_acBuff)
			delete[] m_psz;
	}

	TUnicodeToUTF8(const TUnicodeToUTF8&) = delete;
	TUnicodeToUTF8& operator=(const TUnicodeToUTF8&) = delete;

	operator LPCSTR() const
	{
		return m_psz;
	}

	int GetLength() const
	{
		return m_iChars;
	}

private:
	int m_iChars;
	LPSTR m_psz;
	char m_acBuff[t_nBufferLength];
};

typedef TUnicodeToUTF8<> CUnicodeToUTF8;


///////////////////////////////////////////////////////////////////////////////
// TUnicodeToBOMUTF8

template< int t_nBufferLength = SHORT_ED2K_STR * 4 >
class TUnicodeToBOMUTF8
{
public:
	explicit TUnicodeToBOMUTF8(const CStringW &rwstr)
	{
		int iBuffSize;
		int iMaxEncodedStrSize = 3 + rwstr.GetLength() * 4;
		if (iMaxEncodedStrSize > t_nBufferLength) {
			iBuffSize = iMaxEncodedStrSize;
			m_psz = new char[iBuffSize];
		} else {
			iBuffSize = _countof(m_acBuff);
			m_psz = m_acBuff;
		}

		((UCHAR*)m_psz)[0] = 0xEFU;
		((UCHAR*)m_psz)[1] = 0xBBU;
		((UCHAR*)m_psz)[2] = 0xBFU;
		m_iChars = 3 + AtlUnicodeToUTF8(rwstr, rwstr.GetLength(), m_psz + 3, iBuffSize - 3);
		ASSERT(m_iChars > 3 || rwstr.GetLength() == 0);
	}

	~TUnicodeToBOMUTF8()
	{
		if (m_psz != m_acBuff)
			delete[] m_psz;
	}

	TUnicodeToBOMUTF8(const TUnicodeToBOMUTF8&) = delete;
	TUnicodeToBOMUTF8& operator=(const TUnicodeToBOMUTF8&) = delete;

	operator LPCSTR() const
	{
		return m_psz;
	}

	int GetLength() const
	{
		return m_iChars;
	}

private:
	char m_acBuff[t_nBufferLength];
	LPSTR m_psz;
	int m_iChars;
};

typedef TUnicodeToBOMUTF8<> CUnicodeToBOMUTF8;


///////////////////////////////////////////////////////////////////////////////
// TUnicodeToMultiByte

template< int t_nBufferLength = SHORT_ED2K_STR * 2 >
class TUnicodeToMultiByte
{
public:
	explicit TUnicodeToMultiByte(const CStringW &rwstr, UINT uCodePage = _AtlGetConversionACP())
	{
		int iBuffSize;
		int iMaxEncodedStrSize = rwstr.GetLength() * 2;
		if (iMaxEncodedStrSize > t_nBufferLength) {
			iBuffSize = iMaxEncodedStrSize;
			m_psz = new char[iBuffSize];
		} else {
			iBuffSize = _countof(m_acBuff);
			m_psz = m_acBuff;
		}

		m_iChars = WideCharToMultiByte(uCodePage, 0, rwstr, rwstr.GetLength(), m_psz, iBuffSize, NULL, 0);
		ASSERT(m_iChars > 0 || rwstr.GetLength() == 0);
	}

	~TUnicodeToMultiByte()
	{
		if (m_psz != m_acBuff)
			delete[] m_psz;
	}

	TUnicodeToMultiByte(const TUnicodeToMultiByte&) = delete;
	TUnicodeToMultiByte& operator=(const TUnicodeToMultiByte&) = delete;

	operator LPCSTR() const
	{
		return m_psz;
	}

	int GetLength() const
	{
		return m_iChars;
	}

private:
	int m_iChars;
	LPSTR m_psz;
	char m_acBuff[t_nBufferLength];
};

typedef TUnicodeToMultiByte<> CUnicodeToMultiByte;