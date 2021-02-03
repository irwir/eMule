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

#pragma once
#include "ResizableLib/ResizablePage.h"
#include "ListCtrlX.h"
#include "ArchiveRecovery.h"

class CKnownFile;

static void FreeMemory(void *arg);

///////////////////////////////////////////////////////////////////////////////
// CArchivePreviewDlg

class CArchivePreviewDlg : public CResizablePage
{
	DECLARE_DYNAMIC(CArchivePreviewDlg)

	enum
	{
		IDD = IDD_ARCHPREV
	};
	CShareableFile *m_pFile; //archive contents was displayed for this file
public:
	CArchivePreviewDlg();
	virtual	~CArchivePreviewDlg() = default;
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void SetReducedDialog()									{ m_bReducedDlg = true; }
	void Localize();

protected:
	const CSimpleArray<CObject*> *m_paFiles;
	archiveScannerThreadParams_s *m_activeTParams;

	CListCtrlX	m_ContentList;
	int			m_StoredColWidth2, m_StoredColWidth5;
	bool		m_bDataChanged;
	bool		m_bReducedDlg;

	void UpdateArchiveDisplay(bool doscan);
	int ShowISOResults(int succ, archiveScannerThreadParams_s *tp);
	int ShowZipResults(int succ, archiveScannerThreadParams_s *tp);
	int ShowRarResults(int succ, archiveScannerThreadParams_s *tp);
	int ShowAceResults(int succ, archiveScannerThreadParams_s *tp);
	LRESULT ShowScanResults(WPARAM, LPARAM lParam);

	static UINT RunArchiveScanner(LPVOID pParam);

	virtual void DoDataExchange(CDataExchange *pDX);	// DDX/DDV support
	virtual BOOL OnSetActive();

	CProgressCtrl m_progressbar;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedRead();
	afx_msg void OnBnClickedCreateRestored();
	afx_msg void OnBnExplain();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnDestroy();
	afx_msg void OnLvnDeleteAllItemsArchiveEntries(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNMCustomDrawArchiveEntries(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
};