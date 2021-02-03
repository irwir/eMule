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
#include "ListViewWalkerPropertySheet.h"
#include "FileDetailDialogInfo.h"
#include "FileDetailDialogName.h"
#include "CommentDialogLst.h"
#include "FileInfoDialog.h"
#include "MetaDataDlg.h"
#include "ED2kLinkDlg.h"
#include "ArchivePreviewDlg.h"

class CFileDetailDialog : public CListViewWalkerPropertySheet
{
	DECLARE_DYNAMIC(CFileDetailDialog)

	void Localize();
public:
	explicit CFileDetailDialog(const CSimpleArray<CPartFile*> *paFiles, UINT uInvokePage = 0, CListCtrlItemWalk *pListCtrl = NULL);
	virtual	~CFileDetailDialog() = default;

	virtual BOOL OnInitDialog();

protected:
	CArchivePreviewDlg		m_wndArchiveInfo;
	CCommentDialogLst		m_wndComments;
	CED2kLinkDlg			m_wndFileLink;
	CFileDetailDialogInfo	m_wndInfo;
	CFileDetailDialogName	m_wndName;
	CFileInfoDialog			m_wndMediaInfo;
	CMetaDataDlg			m_wndMetaData;

	UINT m_uInvokePage;
	static LPCTSTR m_pPshStartPage;

	void UpdateTitle();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
};