//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "abstractfile.h"

// A file we know on the file system we could share, but don't do so yet

enum EFileType: uint8;

class CShareableFile : public CAbstractFile
{
	DECLARE_DYNAMIC(CShareableFile)

public:
	CShareableFile();
	virtual	~CShareableFile() = default;
	virtual void UpdateFileRatingCommentAvail(bool /*bForceUpdate = false*/)	{ ASSERT(0); }

	EFileType	GetVerifiedFileType() const			{ return m_verifiedFileType; }
	void SetVerifiedFileType(EFileType in)			{ m_verifiedFileType = in; }

	const CString& GetPath() const					{ return m_strDirectory; }
	void SetPath(LPCTSTR path)						{ m_strDirectory = path; }

	// Shared directory is equal to the path for all shared files, except those following a shell link
	// in which case the directory of the shell link is return. Use GetPath to access the file
	const CString& GetSharedDirectory() const		{ return m_strSharedDirectory.IsEmpty() ? m_strDirectory : m_strSharedDirectory; }
	void SetSharedDirectory(LPCTSTR path)			{ m_strSharedDirectory = path; }
	bool IsShellLinked() const						{ return !m_strSharedDirectory.IsEmpty(); }

	const CString& GetFilePath() const				{ return m_strFilePath; }
	void SetFilePath(LPCTSTR pszFilePath)			{ m_strFilePath = pszFilePath; }
	virtual CString	GetInfoSummary(bool bNoFormatCommands = false) const;

protected:
	CString		m_strDirectory;
	CString		m_strFilePath;
	CString		m_strSharedDirectory;
	EFileType	m_verifiedFileType;
};