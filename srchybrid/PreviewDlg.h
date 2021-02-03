//this file is part of eMule
//Copyright (C)2003 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

class CSearchFile;

class PreviewDlg : public CDialog
{
	DECLARE_DYNAMIC(PreviewDlg)

	enum
	{
		IDD = IDD_PREVIEWDIALOG
	};

public:
	explicit PreviewDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~PreviewDlg();

	void	SetFile(const CSearchFile *pFile)	{ m_pFile = pFile; Show(); }
	void	Show();
protected:
	const CSearchFile *m_pFile;
	int m_nCurrentImage;
	CStatic m_ImageStatic;
	HICON m_icons[3];

	void	ShowImage(int nNumber);

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnClose();
	afx_msg void OnBnClickedPvExit();
	afx_msg void OnBnClickedPvNext();
	afx_msg void OnBnClickedPvPrior();
};