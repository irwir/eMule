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

class CPartFile;

///////////////////////////////////////////////////////////////////////////////
// CPreviewThread

class CPreviewThread : public CWinThread
{
	DECLARE_DYNCREATE(CPreviewThread)

public:
	virtual	BOOL	InitInstance();
	virtual int		Run();
	void	SetValues(CPartFile *pPartFile, LPCTSTR pszCommand, LPCTSTR pszCommandArgs);

protected:
	CPreviewThread();			// protected constructor used by dynamic creation
	virtual	~CPreviewThread() = default;

	CPartFile	*m_pPartfile;
	CString		m_strCommand;
	CString		m_strCommandArgs;

	DECLARE_MESSAGE_MAP()
};


///////////////////////////////////////////////////////////////////////////////
// CPreviewApps

class CPreviewApps
{
public:
	CPreviewApps();

	static CString GetDefaultAppsFile();
	INT_PTR ReadAllApps();
	void RemoveAllApps();

	int GetAllMenuEntries(CMenu &rMenu, const CPartFile *file);
	void RunApp(CPartFile *file, UINT uMenuID) const;

	enum ECanPreviewRes
	{
		NotHandled,
		No,
		Yes
	};
	ECanPreviewRes CanPreview(const CPartFile *file);
	int GetPreviewApp(const CPartFile *file);
	bool Preview(CPartFile *file);

protected:
	struct SPreviewApp
	{
		SPreviewApp() = default;
		SPreviewApp(const SPreviewApp &rCopy)
		{
			*this = rCopy;
		}

		SPreviewApp& operator=(const SPreviewApp &rCopy)
		{
			strTitle = rCopy.strTitle;
			strCommand = rCopy.strCommand;
			strCommandArgs = rCopy.strCommandArgs;
			astrExtensions.Copy(rCopy.astrExtensions);
			ullMinStartOfFile = rCopy.ullMinStartOfFile;
			ullMinCompletedSize = rCopy.ullMinCompletedSize;
			return *this;
		}

		uint64 ullMinStartOfFile;
		uint64 ullMinCompletedSize;
		CString strTitle;
		CString strCommand;
		CString strCommandArgs;
		CStringArray astrExtensions;
	};
	CArray<SPreviewApp> m_aApps;
	time_t m_tDefAppsFileLastModified;
	CPartFile *m_pLastCheckedPartFile;
	SPreviewApp *m_pLastPartFileApp;

	void UpdateApps();

};

extern CPreviewApps thePreviewApps;

void ExecutePartFile(CPartFile *file, LPCTSTR pszCommand, LPCTSTR pszCommandArgs);