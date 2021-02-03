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
#include "MuleListCtrl.h"

struct FCtrlItem_Struct
{
	CString	filename;
	uint16	count;
};

class CFileDetailDialogName : public CResizablePage
{
	DECLARE_DYNAMIC(CFileDetailDialogName)

	enum
	{
		IDD = IDD_FILEDETAILS_NAME
	};

public:
	CFileDetailDialogName();   // standard constructor
	virtual	~CFileDetailDialogName() = default;
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void Localize();

protected:
	CMuleListCtrl m_listFileNames;
	const CSimpleArray<CObject*> *m_paFiles;

	UINT_PTR m_timer;
	int m_aiColWidths[2];
	bool m_bDataChanged;
	bool m_bAppliedSystemImageList;
	bool m_bSelf;

	void RefreshData();
	void FillSourcenameList();
	void Copy();
	bool CanRenameFile() const;
	void RenameFile();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnSetActive();
	virtual BOOL OnApply();

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	static int CALLBACK CompareListNameItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnBnClickedButtonStrip();
	afx_msg void OnDestroy();
	afx_msg void OnEnChangeFilename();
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClkList(LPNMHDR, LRESULT *pResult);
	afx_msg void OnNmRClickList(LPNMHDR, LRESULT *pResult);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void TakeOver();
};