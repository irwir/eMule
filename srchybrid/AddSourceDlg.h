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
#include "ResizableLib/ResizableDialog.h"
#include "otherfunctions.h"

class CPartFile;

// CAddSourceDlg dialog

class CAddSourceDlg : public CResizableDialog
{
	DECLARE_DYNAMIC(CAddSourceDlg)

	enum
	{
		IDD = IDD_ADDSOURCE
	};

public:
	explicit CAddSourceDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CAddSourceDlg() = default;

	void SetFile(CPartFile *pFile);

protected:
	CPartFile *m_pFile;
	int m_nSourceType;

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedRadio1();
//	afx_msg void OnBnClickedRadio2();
//	afx_msg void OnBnClickedRadio3();
	afx_msg void OnBnClickedRadio4();
//	afx_msg void OnBnClickedRadio5();
//	afx_msg void OnBnClickedRadio6();
//	afx_msg void OnBnClickedRadio7();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedOk();
};