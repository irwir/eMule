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
#include "stdafx.h"
#include "emule.h"
#include "FileDetailDialogName.h"
#include "UserMsgs.h"
#include "PartFile.h"
#include "UpDownClient.h"
#include "TitleMenu.h"
#include "MenuCmds.h"
#include "StringConversion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	IDT_REFRESH	301

IMPLEMENT_DYNAMIC(CFileDetailDialogName, CResizablePage)

BEGIN_MESSAGE_MAP(CFileDetailDialogName, CResizablePage)
	ON_BN_CLICKED(IDC_BUTTONSTRIP, OnBnClickedButtonStrip)
	ON_BN_CLICKED(IDC_TAKEOVER, TakeOver)
	ON_EN_CHANGE(IDC_FILENAME, OnEnChangeFilename)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LISTCTRLFILENAMES, OnLvnColumnClick)
	ON_NOTIFY(NM_DBLCLK, IDC_LISTCTRLFILENAMES, OnNmDblClkList)
	ON_NOTIFY(NM_RCLICK, IDC_LISTCTRLFILENAMES, OnNmRClickList)
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()

CFileDetailDialogName::CFileDetailDialogName()
	: CResizablePage(CFileDetailDialogName::IDD)
	, m_paFiles()
	, m_timer()
	, m_aiColWidths()
	, m_bDataChanged()
	, m_bAppliedSystemImageList()
	, m_bSelf()
{
	m_strCaption = GetResString(IDS_SW_NAME);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CFileDetailDialogName::OnTimer(UINT_PTR /*nIDEvent*/)
{
	RefreshData();
}

void CFileDetailDialogName::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LISTCTRLFILENAMES, m_listFileNames);
}

BOOL CFileDetailDialogName::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_FD_SN, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_LISTCTRLFILENAMES, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_TAKEOVER, BOTTOM_LEFT);
	AddAnchor(IDC_BUTTONSTRIP, BOTTOM_RIGHT);
	AddAnchor(IDC_FILENAME, BOTTOM_LEFT, BOTTOM_RIGHT);

	m_listFileNames.SetPrefsKey(_T("FileDetailDlgName"));
	m_listFileNames.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_listFileNames.InsertColumn(0, GetResString(IDS_DL_FILENAME), LVCFMT_LEFT, /*DFLT_FILENAME_COL_WIDTH*/450);
	m_listFileNames.InsertColumn(1, GetResString(IDS_DL_SOURCES), LVCFMT_LEFT, 60);
	ASSERT((m_listFileNames.GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	m_listFileNames.LoadSettings();

	m_listFileNames.SetSortArrow();
	m_listFileNames.SortItems(&CompareListNameItems, m_listFileNames.GetSortItem() + (m_listFileNames.GetSortAscending() ? 0 : 10));

	Localize();

	// start timer for calling 'RefreshData'
	VERIFY((m_timer = SetTimer(IDT_REFRESH, SEC2MS(5), NULL)) != 0);

	return TRUE;
}

BOOL CFileDetailDialogName::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_bDataChanged) {
		m_bSelf = true;
		SetDlgItemText(IDC_FILENAME, static_cast<CPartFile*>((*m_paFiles)[0])->GetFileName());
		m_bSelf = false;
		RefreshData();
		m_bDataChanged = false;
	}
	return TRUE;
}

LRESULT CFileDetailDialogName::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CFileDetailDialogName::RefreshData()
{
	bool bEnableRename = CanRenameFile();
	GetDlgItem(IDC_FILENAME)->EnableWindow(bEnableRename);
	GetDlgItem(IDC_BUTTONSTRIP)->EnableWindow(bEnableRename);
	GetDlgItem(IDC_TAKEOVER)->EnableWindow(bEnableRename);

	FillSourcenameList();
}

void CFileDetailDialogName::OnDestroy()
{
	m_listFileNames.SaveSettings();

	for (int i = m_listFileNames.GetItemCount(); --i >= 0;)
		delete reinterpret_cast<FCtrlItem_Struct*>(m_listFileNames.GetItemData(i));

	if (m_timer) {
		KillTimer(m_timer);
		m_timer = 0;
	}
}

void CFileDetailDialogName::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_SW_NAME, this);

	SetDlgItemText(IDC_TAKEOVER, GetResString(IDS_TAKEOVER));
	SetDlgItemText(IDC_BUTTONSTRIP, GetResString(IDS_CLEANUP));
	SetDlgItemText(IDC_FD_SN, GetResString(IDS_SOURCENAMES));
}

void CFileDetailDialogName::FillSourcenameList()
{
	LVFINDINFO info;
	info.flags = LVFI_STRING;

	// reset
	for (int i = m_listFileNames.GetItemCount(); --i >= 0;)
		reinterpret_cast<FCtrlItem_Struct*>(m_listFileNames.GetItemData(i))->count = 0;

	// update
	const CPartFile *file = static_cast<CPartFile*>((*m_paFiles)[0]);
	for (POSITION pos = file->srclist.GetHeadPosition(); pos != NULL; ) {
		CUpDownClient *cur_src = file->srclist.GetNext(pos);
		if (cur_src->GetRequestFile() != file || cur_src->GetClientFilename().IsEmpty())
			continue;

		info.psz = cur_src->GetClientFilename();
		int itempos = m_listFileNames.FindItem(&info, -1);
		if (itempos == -1) {
			FCtrlItem_Struct *newitem = new FCtrlItem_Struct();
			newitem->count = 1;
			newitem->filename = cur_src->GetClientFilename();

			int iSystemIconIdx = theApp.GetFileTypeSystemImageIdx(cur_src->GetClientFilename());
			if (theApp.GetSystemImageList() && !m_bAppliedSystemImageList) {
				m_listFileNames.ApplyImageList(theApp.GetSystemImageList());
				ASSERT((m_listFileNames.GetStyle() & LVS_SHAREIMAGELISTS) != 0);
				m_bAppliedSystemImageList = true;
			}

			int ix = m_listFileNames.InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE, m_listFileNames.GetItemCount(), cur_src->GetClientFilename(), 0, 0, iSystemIconIdx, (LPARAM)newitem);
			m_listFileNames.SetItemText(ix, 1, _T("1"));
		} else {
			FCtrlItem_Struct *item = reinterpret_cast<FCtrlItem_Struct*>(m_listFileNames.GetItemData(itempos));
			item->count++;
			CString strText;
			strText.Format(_T("%i"), item->count);
			m_listFileNames.SetItemText(itempos, 1, strText);
		}
	}

	// remove 0'er
	for (int i = m_listFileNames.GetItemCount(); --i >= 0;) {
		FCtrlItem_Struct *item = reinterpret_cast<FCtrlItem_Struct*>(m_listFileNames.GetItemData(i));
		if (item && item->count == 0) {
			delete item;
			m_listFileNames.DeleteItem(i);
		}
	}

	m_listFileNames.SortItems(&CompareListNameItems, m_listFileNames.GetSortItem() + (m_listFileNames.GetSortAscending() ? 0 : 10));
}

void CFileDetailDialogName::TakeOver()
{
	int iSel = m_listFileNames.GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0)
		SetDlgItemText(IDC_FILENAME, m_listFileNames.GetItemText(iSel, 0));
}

void CFileDetailDialogName::Copy()
{
	int iSel = m_listFileNames.GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0)
		theApp.CopyTextToClipboard(m_listFileNames.GetItemText(iSel, 0));
}

void CFileDetailDialogName::OnBnClickedButtonStrip()
{
	CString filename;
	GetDlgItemText(IDC_FILENAME, filename);
	SetDlgItemText(IDC_FILENAME, CleanupFilename(filename));
}

void CFileDetailDialogName::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (m_listFileNames.GetSortItem() == pNMListView->iSubItem)
		sortAscending = !m_listFileNames.GetSortAscending();
	else {
		switch (pNMListView->iSubItem) {
		case 1: // Count
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}
	}

	m_listFileNames.SetSortArrow(pNMListView->iSubItem, sortAscending);
	m_listFileNames.SortItems(&CompareListNameItems, pNMListView->iSubItem + (sortAscending ? 0 : 10));

	*pResult = 0;
}

int CALLBACK CFileDetailDialogName::CompareListNameItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const FCtrlItem_Struct *item1 = reinterpret_cast<FCtrlItem_Struct*>(lParam1);
	const FCtrlItem_Struct *item2 = reinterpret_cast<FCtrlItem_Struct*>(lParam2);

	int iResult;
	switch (lParamSort) {
	case 0:
	case 10:
		iResult = CompareLocaleStringNoCase(item1->filename, item2->filename);
		break;
	case 1:
	case 11:
		iResult = CompareUnsigned(item1->count, item2->count);
		break;
	default:
		iResult = 0;
	}
	return (lParamSort < 10) ? iResult : -iResult;
}

void CFileDetailDialogName::OnNmDblClkList(LPNMHDR, LRESULT *pResult)
{
	TakeOver();
	*pResult = 0;
}

void CFileDetailDialogName::OnNmRClickList(LPNMHDR, LRESULT *pResult)
{
	UINT flag = MF_STRING;
	if (m_listFileNames.GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED) == -1)
		flag = MF_GRAYED;

	POINT point;
	::GetCursorPos(&point);
	CTitleMenu popupMenu;
	popupMenu.CreatePopupMenu();
	popupMenu.AppendMenu(flag, MP_MESSAGE, GetResString(IDS_TAKEOVER));
	popupMenu.AppendMenu(flag, MP_COPYSELECTED, GetResString(IDS_COPY));
	popupMenu.AppendMenu(MF_STRING, MP_RESTORE, GetResString(IDS_SV_UPDATE));
	popupMenu.SetDefaultItem(MP_MESSAGE);
	popupMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	VERIFY(popupMenu.DestroyMenu());

	*pResult = 0;
}

BOOL CFileDetailDialogName::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int iSel = m_listFileNames.GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		switch (wParam) {
		case MP_MESSAGE:
			TakeOver();
			return true;
		case MP_COPYSELECTED:
			Copy();
			return true;
		case MP_RESTORE:
			FillSourcenameList();
			return true;
		}
	}
	return CResizablePage::OnCommand(wParam, lParam);
}

void CFileDetailDialogName::RenameFile()
{
	if (CanRenameFile()) {
		CString strNewFileName;
		GetDlgItemText(IDC_FILENAME, strNewFileName);
		strNewFileName.Trim();
		if (strNewFileName.IsEmpty() || !IsValidEd2kString(strNewFileName))
			return;
		CPartFile *file = static_cast<CPartFile*>((*m_paFiles)[0]);
		file->SetFileName(strNewFileName, true);
		file->UpdateDisplayedInfo();
		file->SavePartFile();
		GetParent()->SendMessage(UM_DATA_CHANGED); //refresh notification for FileDetailDialog
	}
}

bool CFileDetailDialogName::CanRenameFile() const
{
	const CPartFile *file = static_cast<CPartFile*>((*m_paFiles)[0]);
	return (file->GetStatus() != PS_COMPLETE && file->GetStatus() != PS_COMPLETING);
}

void CFileDetailDialogName::OnEnChangeFilename()
{
	if (!m_bSelf)
		SetModified();
}

BOOL CFileDetailDialogName::OnApply()
{
	if (!m_bDataChanged)
		RenameFile();
	return CResizablePage::OnApply();
}