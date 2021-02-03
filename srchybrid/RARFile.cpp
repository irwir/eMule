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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "RARFile.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CRARFile::CRARFile()
	: m_hLibUnRar(NULL)
	, m_hArchive(NULL)
	, m_pfnRAROpenArchiveEx(NULL)
	, m_pfnRARCloseArchive(NULL)
	, m_pfnRARReadHeaderEx(NULL)
	, m_pfnRARProcessFileW(NULL)
{
}

CRARFile::~CRARFile()
{
	Close();
	if (m_hLibUnRar)
		VERIFY(FreeLibrary(m_hLibUnRar));
}

bool CRARFile::InitUnRarLib()
{
	if (m_hLibUnRar == NULL) {
		m_hLibUnRar = LoadLibrary(_T("unrar.dll"));
		if (m_hLibUnRar) {
			(FARPROC&)m_pfnRAROpenArchiveEx = GetProcAddress(m_hLibUnRar, "RAROpenArchiveEx");
			(FARPROC&)m_pfnRARCloseArchive = GetProcAddress(m_hLibUnRar, "RARCloseArchive");
			(FARPROC&)m_pfnRARReadHeaderEx = GetProcAddress(m_hLibUnRar, "RARReadHeaderEx");
			(FARPROC&)m_pfnRARProcessFileW = GetProcAddress(m_hLibUnRar, "RARProcessFileW");
			if (m_pfnRAROpenArchiveEx == NULL
				|| m_pfnRARCloseArchive == NULL
				|| m_pfnRARReadHeaderEx == NULL
				|| m_pfnRARProcessFileW == NULL)
			{
				FreeLibrary(m_hLibUnRar);
				m_hLibUnRar = NULL;
			}
		}
	}

	if (m_hLibUnRar == NULL)
		LogWarning(LOG_STATUSBAR, _T("Failed to initialize UNRAR.DLL. Download latest version of UNRAR.DLL from http://www.rarlab.com and copy UNRAR.DLL into eMule installation folder."));

	return m_hLibUnRar != NULL;
}

bool CRARFile::Open(LPCTSTR pszArchiveFilePath)
{
	if (!InitUnRarLib())
		return false;
	Close();

	m_strArchiveFilePath = pszArchiveFilePath;
	RAROpenArchiveDataEx OpenArchiveData = {};
	OpenArchiveData.ArcNameW = const_cast<LPWSTR>((LPCWSTR)m_strArchiveFilePath);
	OpenArchiveData.OpenMode = RAR_OM_EXTRACT;
	try {
		m_hArchive = (*m_pfnRAROpenArchiveEx)(&OpenArchiveData);
	} catch (...) {
		m_hArchive = NULL;
	}

	if (m_hArchive == NULL || OpenArchiveData.OpenResult != 0) {
		Close();
		return false;
	}
	return true;
}

void CRARFile::Close()
{
	if (m_hArchive) {
		ASSERT(m_pfnRARCloseArchive);
		if (m_pfnRARCloseArchive) {
			try {
				VERIFY((*m_pfnRARCloseArchive)(m_hArchive) == 0);
			} catch (...) {
			}
		}
		m_hArchive = NULL;
	}
	m_strArchiveFilePath.Empty();
}

bool CRARFile::GetNextFile(CString &strFile) const
{
	if (m_hLibUnRar == NULL || m_pfnRARReadHeaderEx == NULL || m_hArchive == NULL) {
		ASSERT(0);
		return false;
	}

	struct RARHeaderDataEx HeaderData = {};
	int iReadHeaderResult;
	try {
		iReadHeaderResult = (*m_pfnRARReadHeaderEx)(m_hArchive, &HeaderData);
	} catch (...) {
		return false;
	}
	if (iReadHeaderResult != 0)
		return false;
	strFile = HeaderData.FileNameW;
	return !strFile.IsEmpty();
}

bool CRARFile::Extract(LPCTSTR pszDstFilePath) const
{
	if (m_hLibUnRar == NULL || m_pfnRARProcessFileW == NULL || m_hArchive == NULL) {
		ASSERT(0);
		return false;
	}

	int iProcessFileResult;
	try {
		iProcessFileResult = (*m_pfnRARProcessFileW)(m_hArchive, RAR_EXTRACT, NULL, pszDstFilePath);
	} catch (...) {
		return false;
	}
	return (iProcessFileResult == 0);
}

bool CRARFile::Skip() const
{
	if (m_hLibUnRar == NULL || m_pfnRARProcessFileW == NULL || m_hArchive == NULL) {
		ASSERT(0);
		return false;
	}

	int iProcessFileResult;
	try {
		iProcessFileResult = (*m_pfnRARProcessFileW)(m_hArchive, RAR_SKIP, NULL, NULL);
	} catch (...) {
		return false;
	}
	return (iProcessFileResult == 0);
}