//this file is part of eMule
//Copyright (C)2002-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "CollectionListCtrl.h"
#include "ResizableLib\ResizableDialog.h"

class CCollection;
class CCollectionFile;

class CCollectionCreateDialog : public CResizableDialog
{
	DECLARE_DYNAMIC(CCollectionCreateDialog)

	enum
	{
		IDD = IDD_COLLECTIONCREATEDIALOG
	};

public:
	explicit CCollectionCreateDialog(CWnd *pParent = NULL);   // standard constructor
	virtual	~CCollectionCreateDialog();

	void SetCollection(CCollection *pCollection, bool create);

private:
	enum ECols
	{
		colName = 0,
		colSize,
		colHash
	};

protected:
	CCollection *m_pCollection;
	CEdit m_CollectionNameEdit;
	CCollectionListCtrl m_CollectionListCtrl;
	CCollectionListCtrl m_CollectionAvailListCtrl;
	bool m_bSharedFiles;
	CButton m_CollectionViewShareButton;
	CButton m_CollectionCreateFormatCheck;
	HICON	m_icoWnd;
	HICON	m_icoForward;
	HICON	m_icoBack;
	HICON	m_icoColl;
	HICON	m_icoFiles;
	bool	m_bCreatemode;
	CButton m_AddCollectionButton;
	CButton m_RemoveCollectionButton;
	CStatic m_CollectionListLabel;
	CButton m_SaveButton;
	CButton m_CancelButton;
	CStatic m_CollectionListIcon;
	CStatic m_CollectionSourceListIcon;
	CButton m_CollectionCreateSignNameKeyCheck;

	void AddSelectedFiles();
	void RemoveSelectedFiles();
	void UpdateAvailFiles();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedCollectionAdd();
	afx_msg void OnBnClickedCollectionFormat();
	afx_msg void OnBnClickedCollectionRemove();
	afx_msg void OnBnClickedCollectionViewShared();
	afx_msg void OnBnClickedOk();
	afx_msg void OnEnKillFocusCollectionName();
	afx_msg void OnNmDblClkCollectionAvailList(LPNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClkCollectionList(LPNMHDR, LRESULT *pResult);
};