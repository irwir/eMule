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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "CommentListCtrl.h"
#include "MenuCmds.h"
#include "TitleMenu.h"
#include "emule.h"
#include "UpDownClient.h"
#include "kademlia/kademlia/Entry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

IMPLEMENT_DYNAMIC(CCommentListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CCommentListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteItem)
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

void CCommentListCtrl::Init()
{
	SetPrefsKey(_T("CommentListCtrl"));
	ASSERT((GetStyle() & LVS_SINGLESEL) == 0);
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(colRating,		GetResString(IDS_QL_RATING),	LVCFMT_LEFT,  80);
	InsertColumn(colComment,	GetResString(IDS_COMMENT),		LVCFMT_LEFT, 340);
	InsertColumn(colFileName,	GetResString(IDS_DL_FILENAME),	LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	InsertColumn(colUserName,	GetResString(IDS_QL_USERNAME),	LVCFMT_LEFT, DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(colOrigin,		GetResString(IDS_NETWORK),		LVCFMT_LEFT,  80);

	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("Rating_NotRated")));
	iml.Add(CTempIconLoader(_T("Rating_Fake")));
	iml.Add(CTempIconLoader(_T("Rating_Poor")));
	iml.Add(CTempIconLoader(_T("Rating_Fair")));
	iml.Add(CTempIconLoader(_T("Rating_Good")));
	iml.Add(CTempIconLoader(_T("Rating_Excellent")));
	CImageList *pimlOld = SetImageList(&iml, LVSIL_SMALL);
	iml.Detach();
	if (pimlOld)
		pimlOld->DeleteImageList();

	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, MAKELONG(GetSortItem(), static_cast<int>(!GetSortAscending())));
}

int CALLBACK CCommentListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const SComment *item1 = reinterpret_cast<SComment*>(lParam1);
	const SComment *item2 = reinterpret_cast<SComment*>(lParam2);
	if (item1 == NULL || item2 == NULL)
		return 0;

	int iResult = 0;
	switch (LOWORD(lParamSort)) {
	case colRating:
		if (item1->m_iRating < item2->m_iRating)
			iResult = -1;
		else if (item1->m_iRating > item2->m_iRating)
			iResult = 1;
		break;
	case colComment:
		iResult = CompareLocaleStringNoCase(item1->m_strComment, item2->m_strComment);
		break;
	case colFileName:
		iResult = CompareLocaleStringNoCase(item1->m_strFileName, item2->m_strFileName);
		break;
	case colUserName:
		iResult = CompareLocaleStringNoCase(item1->m_strUserName, item2->m_strUserName);
		break;
	case colOrigin:
		if (item1->m_iOrigin < item2->m_iOrigin)
			iResult = -1;
		else if (item1->m_iOrigin > item2->m_iOrigin)
			iResult = 1;
		break;
	default:
		ASSERT(0);
	}
	return HIWORD(lParamSort) ? -iResult : iResult;
}

void CCommentListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	// Determine ascending based on whether already sorted on this column
	int iSortItem = GetSortItem();
	bool bOldSortAscending = GetSortAscending();
	bool bSortAscending = (iSortItem != pNMLV->iSubItem) ? true : !bOldSortAscending;

	// Item is column clicked
	iSortItem = pNMLV->iSubItem;

	// Sort table
	UpdateSortHistory(MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));
	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));

	*pResult = 0;
}

void CCommentListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	UINT flag = MF_STRING;
	if (GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED) == -1)
		flag = MF_GRAYED;

	CTitleMenu popupMenu;
	popupMenu.CreatePopupMenu();
	popupMenu.AppendMenu(MF_STRING | flag, MP_COPYSELECTED, GetResString(IDS_COPY));

	GetPopupMenuPos(*this, point);
	popupMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	VERIFY(popupMenu.DestroyMenu());
}

BOOL CCommentListCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (wParam == MP_COPYSELECTED) {
		CString strText;
		for (POSITION posItem = GetFirstSelectedItemPosition(); posItem != NULL;) {
			int iItem = GetNextSelectedItem(posItem);
			if (iItem >= 0) {
				const CString &strComment(GetItemText(iItem, colComment));
				if (!strComment.IsEmpty())
					if (strText.IsEmpty())
						strText = strComment;
					else
						strText.AppendFormat(_T("\r\n%s"), (LPCTSTR)strComment);
			}
		}
		theApp.CopyTextToClipboard(strText);
	}
	return CMuleListCtrl::OnCommand(wParam, lParam);
}

int CCommentListCtrl::FindClientComment(const void *pClientCookie)
{
	int iItems = GetItemCount();
	for (int i = 0; i < iItems; ++i) {
		const SComment *pComment = reinterpret_cast<SComment*>(GetItemData(i));
		if (pComment && pComment->m_pClientCookie == pClientCookie)
			return i;
	}
	return -1;
}

void CCommentListCtrl::AddComment(const SComment *pComment)
{
	int iItem = InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM
			, 0, GetRateString(pComment->m_iRating)
			, 0, 0, pComment->m_iRating, (LPARAM)pComment);
	SetItemText(iItem, colComment, pComment->m_strComment);
	SetItemText(iItem, colFileName, pComment->m_strFileName);
	SetItemText(iItem, colUserName, pComment->m_strUserName);
	SetItemText(iItem, colOrigin, pComment->m_iOrigin == 0 ? _T("eD2K") : _T("Kad"));
}

void CCommentListCtrl::AddItem(const CUpDownClient *client)
{
	const void *pClientCookie = client;
	if (FindClientComment(pClientCookie) >= 0)
		return;
	int iRating = client->GetFileRating();
	SComment *pComment = new SComment(pClientCookie, iRating, client->GetFileComment()
		, client->GetClientFilename(), client->GetUserName(), 0/*eD2K*/);
	AddComment(pComment);
}

void CCommentListCtrl::AddItem(const Kademlia::CEntry *entry)
{
	const void *pClientCookie = entry;
	if (FindClientComment(pClientCookie) >= 0)
		return;
	int iRating = (int)entry->GetIntTagValue(Kademlia::CKadTagNameString(TAG_FILERATING));
	SComment *pComment = new SComment(pClientCookie, iRating, entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION))
		, entry->GetCommonFileName(), _T(""), 1/*Kad*/);
	AddComment(pComment);
}

void CCommentListCtrl::OnLvnDeleteItem(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	delete reinterpret_cast<SComment*>(pNMLV->lParam);
	*pResult = 0;
}