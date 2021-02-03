//this file is part of eMule
//Copyright (C)2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

#define ERAR_END_ARCHIVE     10
#define ERAR_NO_MEMORY       11
#define ERAR_BAD_DATA        12
#define ERAR_BAD_ARCHIVE     13
#define ERAR_UNKNOWN_FORMAT  14
#define ERAR_EOPEN           15
#define ERAR_ECREATE         16
#define ERAR_ECLOSE          17
#define ERAR_EREAD           18
#define ERAR_EWRITE          19
#define ERAR_SMALL_BUF       20
#define ERAR_UNKNOWN         21

#define RAR_OM_LIST           0
#define RAR_OM_EXTRACT        1

#define RAR_SKIP              0
#define RAR_TEST              1
#define RAR_EXTRACT           2

#define RAR_VOL_ASK           0
#define RAR_VOL_NOTIFY        1

#define RAR_DLL_VERSION       4

#pragma pack(push, 1)
struct RAROpenArchiveDataEx
{
	char   *ArcName;
	WCHAR  *ArcNameW;
	DWORD	OpenMode;
	DWORD	OpenResult;
	char   *CmtBuf;
	DWORD	CmtBufSize;
	DWORD	CmtSize;
	DWORD	CmtState;
	DWORD	Flags;
	DWORD	Reserved[32];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RARHeaderDataEx
{
	char	ArcName[1024];
	WCHAR	ArcNameW[1024];
	char	FileName[1024];
	WCHAR	FileNameW[1024];
	DWORD	Flags;
	DWORD	PackSize;
	DWORD	PackSizeHigh;
	DWORD	UnpSize;
	DWORD	UnpSizeHigh;
	DWORD	HostOS;
	DWORD	FileCRC;
	DWORD	FileTime;
	DWORD	UnpVer;
	DWORD	Method;
	DWORD	FileAttr;
	char	*CmtBuf;
	DWORD	CmtBufSize;
	DWORD	CmtSize;
	DWORD	CmtState;
	DWORD	Reserved[1024];
};
#pragma pack(pop)

class CRARFile
{
public:
	CRARFile();
	~CRARFile();

	bool Open(LPCTSTR pszArchiveFilePath);
	void Close();
	bool GetNextFile(CString &strFile) const;
	bool Extract(LPCTSTR pszDstFilePath) const;
	bool Skip() const;

protected:
	HMODULE m_hLibUnRar;
	CString m_strArchiveFilePath;
	HANDLE m_hArchive;

	bool InitUnRarLib();
	HANDLE(WINAPI *m_pfnRAROpenArchiveEx)(struct RAROpenArchiveDataEx *ArchiveData) throw(...);
	int (WINAPI *m_pfnRARCloseArchive)(HANDLE hArcData) throw(...);
	int (WINAPI *m_pfnRARReadHeaderEx)(HANDLE hArcData, struct RARHeaderDataEx *HeaderData) throw(...);
	int (WINAPI *m_pfnRARProcessFileW)(HANDLE hArcData, int iOperation, const WCHAR *pszDestFolder, const WCHAR *pszDestName) throw(...);
};