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
#include "UploadListCtrl.h"
#include "DownloadListCtrl.h"
#include "QueueListCtrl.h"
#include "ClientListCtrl.h"
#include "DownloadClientsCtrl.h"

class CTransferWnd;
class CToolbarWnd;

class CTransferDlg : public CFrameWnd
{
	DECLARE_DYNCREATE(CTransferDlg)

public:
	enum EWnd2
	{
		wnd2Downloading = 0,
		wnd2Uploading = 1,
		wnd2OnQueue = 2,
		wnd2Clients = 3
	};

	CTransferDlg();           // protected constructor used by dynamic creation
	virtual	~CTransferDlg();
	CTransferWnd *m_pwndTransfer;

	BOOL Create(CWnd *pParent);

	//Wrappers
	void Localize();
	void ShowQueueCount(INT_PTR number);
	void UpdateFilesCount(int iCount);
	void UpdateCatTabTitles(bool force = true);
	void VerifyCatTabSize();
	int	 AddCategoryInteractive();
	void SwitchUploadList();
	void ResetTransToolbar(bool bShowToolbar, bool bResetLists = true);
	void SetToolTipsDelay(DWORD dwDelay);
	void OnDisableList();
	void UpdateListCount(EWnd2 listindex, int iCount = -1);
	int	 AddCategory(const CString &newtitle, const CString &newincoming, const CString &newcomment, const CString &newautocat, bool addTab = true);
	void ShowToolbar(bool bShow);

	CUploadListCtrl*		GetUploadList();
	CDownloadListCtrl*		GetDownloadList();
	CQueueListCtrl*			GetQueueList();
	CClientListCtrl*		GetClientList();
	CDownloadClientsCtrl*	GetDownloadClientsList();
	CImageList*				GetClientIconList() const;

protected:
	CToolbarWnd *m_pwndToolbar;

	virtual BOOL PreTranslateMessage(MSG *pMsg);
	void DockToolbarWnd();

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnClose();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};