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
#include <fcntl.h>
#include <sys/stat.h>
#include <share.h>
#include <io.h>
#include "zlib/zlib.h"
#include "GZipFile.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CGZIPFile::CGZIPFile()
{
	m_gzFile = 0;
}

bool CGZIPFile::Open(LPCTSTR pszFilePath)
{
	ASSERT(m_gzFile == 0);
	Close();

	m_gzFile = gzopen(CStringA(pszFilePath), "rb");
	if (m_gzFile) {
		// Use gzip-uncompress only for real gzip-compressed files and do not let handle it also uncompressed files.
		// This way the 'Open' function can be used to check if that file is a 'gzip' file at all.
		if (gzdirect(m_gzFile) != 0)
			Close();
		else
			m_strGzFilePath = pszFilePath;
	}
	return m_gzFile != 0;
}

void CGZIPFile::Close()
{
	if (m_gzFile) {
		VERIFY(gzclose(m_gzFile) == Z_OK);
		m_gzFile = 0;
	}
	m_strGzFilePath.Empty();
}

CString CGZIPFile::GetUncompressedFilePath() const
{
	// return path of input file without ".gz" extension
	LPCTSTR pszExt = PathFindExtension(m_strGzFilePath);
	if (_tcsicmp(pszExt, _T(".gz")) != 0)
		pszExt = m_strGzFilePath;
	return CString(m_strGzFilePath, (int)((LPCTSTR)m_strGzFilePath - pszExt));
}

CString CGZIPFile::GetUncompressedFileName() const
{
	// return name (without path) of input file without ".gz" extension
	const CString &strUncompressedFileName = GetUncompressedFilePath();
	if (!strUncompressedFileName.IsEmpty()) {
		// skip any possible available directories
		LPCTSTR pszFileName = PathFindFileName(strUncompressedFileName);
		if (pszFileName)
			return CString(pszFileName);
	}
	return strUncompressedFileName;
}

bool CGZIPFile::Extract(LPCTSTR pszFilePath)
{
	int fdOut = _tsopen(pszFilePath, _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE);
	if (fdOut == -1)
		return false;

	bool bResult = true;
	const int iBuffSize = 32768;
	BYTE *pucBuff = new BYTE[iBuffSize];
	while (!gzeof(m_gzFile)) {
		int iRead = gzread(m_gzFile, pucBuff, iBuffSize);
		if (iRead == 0)
			break;
		if (iRead < 0) {
			bResult = false;
			break;
		}
		if (_write(fdOut, pucBuff, iRead) != iRead) {
			bResult = false;
			break;
		}
	}
	delete[] pucBuff;
	_close(fdOut);
	if (!bResult)
		VERIFY(_tremove(pszFilePath) == 0);
	return bResult;
}