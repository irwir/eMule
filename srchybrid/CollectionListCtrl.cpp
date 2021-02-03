//this file is part of eMule
//Copyright (C)2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
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
#include "stdafx.h"
#include "emule.h"
#include "CollectionListCtrl.h"
#include "OtherFunctions.h"
#include "AbstractFile.h"
#include "MetaDataDlg.h"
#include "HighColorTab.hpp"
#include "ListViewWalkerPropertySheet.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////////////
// CCollectionFileDetailsSheet

class CCollectionFileDetailsSheet : public CListViewWalkerPropertySheet
{
	DECLARE_DYNAMIC(CCollectionFileDetailsSheet)

public:
	explicit CCollectionFileDetailsSheet(CTypedPtrList<CPtrList, CAbstractFile*> &aFiles, UINT uInvokePage = 0, CListCtrlItemWalk *pListCtrl = NULL);
	virtual	~CCollectionFileDetailsSheet() = default;

	virtual BOOL OnInitDialog();

protected:
	CMetaDataDlg		m_wndMetaData;

	UINT m_uInvokePage;
	static LPCTSTR m_pPshStartPage;

	void UpdateTitle();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
};

LPCTSTR CCollectionFileDetailsSheet::m_pPshStartPage;

IMPLEMENT_DYNAMIC(CCollectionFileDetailsSheet, CListViewWalkerPropertySheet)

BEGIN_MESSAGE_MAP(CCollectionFileDetailsSheet, CListViewWalkerPropertySheet)
	ON_WM_DESTROY()
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
END_MESSAGE_MAP()

CCollectionFileDetailsSheet::CCollectionFileDetailsSheet(CTypedPtrList<CPtrList, CAbstractFile*> &aFiles, UINT uInvokePage, CListCtrlItemWalk *pListCtrl)
	: CListViewWalkerPropertySheet(pListCtrl)
	, m_uInvokePage(uInvokePage)
{
	for (POSITION pos = aFiles.GetHeadPosition(); pos != NULL;)
		m_aItems.Add(aFiles.GetNext(pos));
	m_psh.dwFlags &= ~PSH_HASHELP;

	m_wndMetaData.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMetaData.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMetaData.m_psp.pszIcon = _T("METADATA");
	if (thePrefs.IsExtControlsEnabled() && m_aItems.GetSize() == 1) {
		m_wndMetaData.SetFiles(&m_aItems);
		AddPage(&m_wndMetaData);
	}

	LPCTSTR pPshStartPage = m_pPshStartPage;
	if (m_uInvokePage != 0)
		pPshStartPage = MAKEINTRESOURCE(m_uInvokePage);
	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == pPshStartPage) {
			m_psh.nStartPage = i;
			break;
		}
}

void CCollectionFileDetailsSheet::OnDestroy()
{
	if (m_uInvokePage == 0)
		m_pPshStartPage = GetPage(GetActiveIndex())->m_psp.pszTemplate;
	CListViewWalkerPropertySheet::OnDestroy();
}

BOOL CCollectionFileDetailsSheet::OnInitDialog()
{
	EnableStackedTabs(FALSE);
	BOOL bResult = CListViewWalkerPropertySheet::OnInitDialog();
	HighColorTab::UpdateImageList(*this);
	InitWindowStyles(this);
	EnableSaveRestore(_T("CollectionFileDetailsSheet")); // call this after(!) OnInitDialog
	UpdateTitle();
	return bResult;
}

LRESULT CCollectionFileDetailsSheet::OnDataChanged(WPARAM, LPARAM)
{
	UpdateTitle();
	return 1;
}

void CCollectionFileDetailsSheet::UpdateTitle()
{
	CString sTitle(GetResString(IDS_DETAILS));
	if (m_aItems.GetSize() == 1)
		sTitle.AppendFormat(_T(": %s"), (LPCTSTR)(static_cast<CAbstractFile*>(m_aItems[0])->GetFileName()));
	SetWindowText(sTitle);
}



//////////////////////////////////////////////////////////////////////////////
// CCollectionListCtrl

IMPLEMENT_DYNAMIC(CCollectionListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CCollectionListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnNmRClick)
END_MESSAGE_MAP()

CCollectionListCtrl::CCollectionListCtrl()
	: CListCtrlItemWalk(this)
{
}

void CCollectionListCtrl::Init(const CString &strNameAdd)
{
	SetPrefsKey(_T("CollectionListCtrl") + strNameAdd);

	ASSERT(GetStyle() & LVS_SHAREIMAGELISTS);
	SendMessage(LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)theApp.GetSystemImageList());

	ASSERT((GetStyle() & LVS_SINGLESEL) == 0);
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(colName, GetResString(IDS_DL_FILENAME),	LVCFMT_LEFT,  DFLT_FILENAME_COL_WIDTH);
	InsertColumn(colSize, GetResString(IDS_DL_SIZE),		LVCFMT_RIGHT, DFLT_SIZE_COL_WIDTH);
	InsertColumn(colHash, GetResString(IDS_FILEHASH),		LVCFMT_LEFT,  DFLT_HASH_COL_WIDTH);

	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, MAKELONG(GetSortItem(), static_cast<int>(!GetSortAscending())));
}

void CCollectionListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	// Determine ascending based on whether already sorted on this column
	int iSortItem = GetSortItem();
	bool bOldSortAscending = GetSortAscending();
	bool bSortAscending = (iSortItem != pNMListView->iSubItem) || !bOldSortAscending;

	// Item is column clicked
	iSortItem = pNMListView->iSubItem;

	// Sort table
	UpdateSortHistory(MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));
	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));

	*pResult = 0;
}

int CALLBACK CCollectionListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CAbstractFile *item1 = reinterpret_cast<CAbstractFile*>(lParam1);
	const CAbstractFile *item2 = reinterpret_cast<CAbstractFile*>(lParam2);
	if (item1 == NULL || item2 == NULL)
		return 0;

	int iResult;
	switch (LOWORD(lParamSort)) {
	case colName:
		iResult = CompareLocaleStringNoCase(item1->GetFileName(), item2->GetFileName());
		break;

	case colSize:
		iResult = CompareUnsigned64(item1->GetFileSize(), item2->GetFileSize());
		break;

	case colHash:
		iResult = memcmp(item1->GetFileHash(), item2->GetFileHash(), 16);
		break;

	default:
		return 0;
	}
	return HIWORD(lParamSort) ? -iResult : iResult;
}

void CCollectionListCtrl::OnNmRClick(LPNMHDR, LRESULT *pResult)
{
	CTypedPtrList<CPtrList, CAbstractFile*> abstractFileList;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
		int index = GetNextSelectedItem(pos);
		if (index >= 0)
			abstractFileList.AddTail(reinterpret_cast<CAbstractFile*>(GetItemData(index)));
	}

	if (!abstractFileList.IsEmpty()) {
		CCollectionFileDetailsSheet dialog(abstractFileList, 0, this);
		dialog.DoModal();
	}
	*pResult = 0;
}

void CCollectionListCtrl::AddFileToList(CAbstractFile *pAbstractFile)
{
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)pAbstractFile;
	int iItem = FindItem(&find);
	if (iItem >= 0) {
		ASSERT(0);
		return;
	}

	int iImage = theApp.GetFileTypeSystemImageIdx(pAbstractFile->GetFileName());
	iItem = InsertItem(LVIF_TEXT | LVIF_PARAM | (iImage > 0 ? LVIF_IMAGE : 0), GetItemCount(), NULL, 0, 0, iImage, (LPARAM)pAbstractFile);
	if (iItem >= 0) {
		SetItemText(iItem, colName, pAbstractFile->GetFileName());
		SetItemText(iItem, colSize, CastItoXBytes(pAbstractFile->GetFileSize()));
		SetItemText(iItem, colHash, md4str(pAbstractFile->GetFileHash()));
	}
}

void CCollectionListCtrl::RemoveFileFromList(CAbstractFile *pAbstractFile)
{
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)pAbstractFile;
	int iItem = FindItem(&find);
	if (iItem >= 0)
		DeleteItem(iItem);
	else
		ASSERT(0);
}