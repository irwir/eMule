//this file is part of eMule
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
#include "ResizableLib/ResizablePage.h"
#include "RichEditCtrlX.h"

class CKnownFile;
struct SMediaInfo;

/////////////////////////////////////////////////////////////////////////////
// CFileInfoDialog dialog

class CFileInfoDialog : public CResizablePage
{
	DECLARE_DYNAMIC(CFileInfoDialog)

	enum
	{
		IDD = IDD_FILEINFO
	};
	CSimpleArray<CObject*> m_pFiles;
public:
	CFileInfoDialog();   // standard constructor
	virtual	~CFileInfoDialog() = default;
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void SetReducedDialog()									{ m_bReducedDlg = true; }
	void Localize();

protected:
	const CSimpleArray<CObject*> *m_paFiles;
	bool m_bDataChanged;
	CRichEditCtrlX m_fi;
	bool m_bReducedDlg;
	//CHARFORMAT m_cfDef;
	//CHARFORMAT m_cfBold;
	//CHARFORMAT m_cfRed;

	//bool GetMediaInfo(const CKnownFile *file, SMediaInfo *mi, bool bSingleFile);
	//void AddFileInfo(LPCTSTR pszFmt, ...);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnMediaInfoResult(WPARAM, LPARAM);
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnDestroy();
};