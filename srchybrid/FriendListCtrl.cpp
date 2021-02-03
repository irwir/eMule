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
#include "stdafx.h"
#include "emule.h"
#include "FriendListCtrl.h"
#include "friend.h"
#include "ClientDetailDialog.h"
#include "Addfriend.h"
#include "FriendList.h"
#include "emuledlg.h"
#include "ClientList.h"
#include "OtherFunctions.h"
#include "UpDownClient.h"
#include "ListenSocket.h"
#include "MenuCmds.h"
#include "ChatWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CFriendListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CFriendListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CFriendListCtrl::CFriendListCtrl()
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("FriendsLv"));
}

void CFriendListCtrl::Init()
{
	SetPrefsKey(_T("FriendListCtrl"));

	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	CRect rcWindow;
	GetWindowRect(rcWindow);
	InsertColumn(0, GetResString(IDS_QL_USERNAME), LVCFMT_LEFT, rcWindow.Width() - 4);

	SetAllIcons();
	theApp.friendlist->SetWindow(this);
	LoadSettings();
	SetSortArrow();
}

void CFriendListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CFriendListCtrl::SetAllIcons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("FriendNoClient")));
	iml.Add(CTempIconLoader(_T("FriendWithClient")));
	iml.Add(CTempIconLoader(_T("FriendConnected")));
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) == 0);
	HIMAGELIST himlOld = ApplyImageList(iml.Detach());
	if (himlOld)
		::ImageList_Destroy(himlOld);
}

void CFriendListCtrl::Localize()
{
	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	CString strRes(GetResString(IDS_QL_USERNAME));
	HDITEM hdi;
	hdi.mask = HDI_TEXT;
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(0, &hdi);

	for (int i = GetItemCount(); --i >= 0;)
		UpdateFriend(i, reinterpret_cast<CFriend*>(GetItemData(i)));
}

void CFriendListCtrl::UpdateFriend(int iItem, const CFriend *pFriend)
{
	SetItemText(iItem, 0, pFriend->m_strName.IsEmpty() ? _T('(') + GetResString(IDS_UNKNOWN) + _T(')') : pFriend->m_strName);

	int iImage;
	if (!pFriend->GetLinkedClient())
		iImage = 0;
	else if (pFriend->GetLinkedClient()->socket && pFriend->GetLinkedClient()->socket->IsConnected())
		iImage = 2;
	else
		iImage = 1;
	SetItem(iItem, 0, LVIF_IMAGE, 0, iImage, 0, 0, 0, 0);
}

void CFriendListCtrl::AddFriend(const CFriend *pFriend)
{
	int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), pFriend->m_strName, 0, 0, 0, (LPARAM)pFriend);
	if (iItem >= 0)
		UpdateFriend(iItem, pFriend);
	theApp.emuledlg->chatwnd->UpdateFriendlistCount(theApp.friendlist->GetCount());
}

void CFriendListCtrl::RemoveFriend(const CFriend *pFriend)
{
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)pFriend;
	int iItem = FindItem(&find);
	if (iItem >= 0)
		DeleteItem(iItem);
	theApp.emuledlg->chatwnd->UpdateFriendlistCount(theApp.friendlist->GetCount());
}

void CFriendListCtrl::RefreshFriend(const CFriend *pFriend)
{
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)pFriend;
	int iItem = FindItem(&find);
	if (iItem >= 0)
		UpdateFriend(iItem, pFriend);
}

void CFriendListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	CTitleMenu ClientMenu;
	ClientMenu.CreatePopupMenu();
	ClientMenu.AddMenuTitle(GetResString(IDS_FRIENDLIST), true);

	const CFriend *cur_friend = NULL;
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		cur_friend = reinterpret_cast<CFriend*>(GetItemData(iSel));
		ClientMenu.AppendMenu(MF_STRING, MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("CLIENTDETAILS"));
		ClientMenu.SetDefaultItem(MP_DETAIL);
	}

	ClientMenu.AppendMenu(MF_STRING, MP_ADDFRIEND, GetResString(IDS_ADDAFRIEND), _T("ADDFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | (cur_friend ? MF_ENABLED : MF_GRAYED), MP_REMOVEFRIEND, GetResString(IDS_REMOVEFRIEND), _T("DELETEFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | (cur_friend ? MF_ENABLED : MF_GRAYED), MP_MESSAGE, GetResString(IDS_SEND_MSG), _T("SENDMESSAGE"));
	ClientMenu.AppendMenu(MF_STRING | ((cur_friend == NULL || cur_friend->GetLinkedClient(true) && !cur_friend->GetLinkedClient(true)->GetViewSharedFilesSupport()) ? MF_GRAYED : MF_ENABLED), MP_SHOWLIST, GetResString(IDS_VIEWFILES), _T("VIEWFILES"));
	ClientMenu.AppendMenu(MF_STRING, MP_FRIENDSLOT, GetResString(IDS_FRIENDSLOT), _T("FRIENDSLOT"));
	ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));

	ClientMenu.EnableMenuItem(MP_FRIENDSLOT, (cur_friend ? MF_ENABLED : MF_GRAYED));
	ClientMenu.CheckMenuItem(MP_FRIENDSLOT, (cur_friend && cur_friend->GetFriendSlot()) ? MF_CHECKED : MF_UNCHECKED);

	GetPopupMenuPos(*this, point);
	ClientMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

BOOL CFriendListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	CFriend *cur_friend = (iSel >= 0) ? reinterpret_cast<CFriend*>(GetItemData(iSel)) : NULL;

	switch (wParam) {
	case MP_MESSAGE:
		if (cur_friend)
			theApp.emuledlg->chatwnd->StartSession(cur_friend->GetClientForChatSession());
		break;
	case MP_REMOVEFRIEND:
		if (cur_friend) {
			theApp.friendlist->RemoveFriend(cur_friend);
			// auto select next item after deleted one.
			if (iSel < GetItemCount()) {
				SetSelectionMark(iSel);
				SetItemState(iSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			}
			theApp.emuledlg->chatwnd->UpdateSelectedFriendMsgDetails();
		}
		break;
	case MP_ADDFRIEND:
		{
			CAddFriend dialog2;
			dialog2.DoModal();
		}
		break;
	case MP_DETAIL:
	case MPG_ALTENTER:
	case IDA_ENTER:
		if (cur_friend)
			ShowFriendDetails(cur_friend);
		break;
	case MP_SHOWLIST:
		if (cur_friend) {
			if (cur_friend->GetLinkedClient(true))
				cur_friend->GetLinkedClient()->RequestSharedFileList();
			else {
				CUpDownClient *newclient = new CUpDownClient(0, cur_friend->m_nLastUsedPort, cur_friend->m_dwLastUsedIP, 0, 0, true);
				newclient->SetUserName(cur_friend->m_strName);
				theApp.clientlist->AddClient(newclient);
				newclient->RequestSharedFileList();
			}
		}
		break;
	case MP_FRIENDSLOT:
		if (cur_friend) {
			bool bIsAlready = cur_friend->GetFriendSlot();
			theApp.friendlist->RemoveAllFriendSlots();
			if (!bIsAlready)
				cur_friend->SetFriendSlot(true);
		}
		break;
	case MP_FIND:
		OnFindStart();
	}
	return true;
}

void CFriendListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0)
		ShowFriendDetails(reinterpret_cast<CFriend*>(GetItemData(iSel)));
	*pResult = 0;
}

void CFriendListCtrl::ShowFriendDetails(const CFriend *pFriend)
{
	if (pFriend)
		if (pFriend->GetLinkedClient(true)) {
			CClientDetailDialog dlg(pFriend->GetLinkedClient());
			dlg.DoModal();
		} else {
			CAddFriend dlg;
			dlg.m_pShowFriend = const_cast<CFriend*>(pFriend);
			dlg.DoModal();
		}
}

BOOL CFriendListCtrl::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
		switch (pMsg->wParam) {
		case VK_DELETE:
		case VK_INSERT:
			PostMessage(WM_COMMAND, (pMsg->wParam == VK_DELETE ? MP_REMOVEFRIEND : MP_ADDFRIEND), 0);
		}
	return CMuleListCtrl::PreTranslateMessage(pMsg);
}

void CFriendListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	// Determine ascending based on whether already sorted on this column
	int iSortItem = GetSortItem();
	bool bOldSortAscending = GetSortAscending();
	bool bSortAscending = (iSortItem != pNMListView->iSubItem) ? true : !bOldSortAscending;

	// Item is column clicked
	iSortItem = pNMListView->iSubItem;

	// Sort table
	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));

	*pResult = 0;
}

int CALLBACK CFriendListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CFriend *item1 = reinterpret_cast<CFriend*>(lParam1);
	const CFriend *item2 = reinterpret_cast<CFriend*>(lParam2);
	if (item1 == NULL || item2 == NULL)
		return 0;

	int iResult;
	switch (LOWORD(lParamSort)) {
	case 0:
		iResult = CompareLocaleStringNoCase(item1->m_strName, item2->m_strName);
		break;
	default:
		return 0;
	}
	return HIWORD(lParamSort) ? -iResult : iResult;
}

void CFriendListCtrl::UpdateList()
{
	theApp.emuledlg->chatwnd->UpdateFriendlistCount(theApp.friendlist->GetCount());
	SortItems(SortProc, MAKELONG(GetSortItem(), static_cast<int>(!GetSortAscending())));
}