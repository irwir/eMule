//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "ProgressCtrlX.h"
#include "ResizableLib/ResizablePage.h"

class CAbstractFile;

class CFileDetailDlgStatistics : public CResizablePage
{
	DECLARE_DYNAMIC(CFileDetailDlgStatistics)

	enum
	{
		IDD = IDD_FILESTATISTICS
	};

public:
	CFileDetailDlgStatistics();
	virtual	~CFileDetailDlgStatistics() = default;
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void Localize();

protected:
	const CSimpleArray<CObject*> *m_paFiles;
	bool m_bDataChanged;
	CProgressCtrlX pop_bar;
	CProgressCtrlX pop_baraccept;
	CProgressCtrlX pop_bartrans;
	CProgressCtrlX pop_bar2;
	CProgressCtrlX pop_baraccept2;
	CProgressCtrlX pop_bartrans2;

	void RefreshData();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnSysColorChange();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	uint32 nLastRequestCount;
	UINT_PTR m_hRefreshTimer;
};
