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
#include "ClientListCtrl.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "KademliaWnd.h"
#include "ClientList.h"
#include "emuledlg.h"
#include "FriendList.h"
#include "TransferDlg.h"
#include "MemDC.h"
#include "UpDownClient.h"
#include "ClientCredits.h"
#include "ListenSocket.h"
#include "ChatWnd.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/net/KademliaUDPListener.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CClientListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CClientListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CClientListCtrl::CClientListCtrl()
	: CListCtrlItemWalk(this)
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("ClientsLv"));
}

void CClientListCtrl::Init()
{
	SetPrefsKey(_T("ClientListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(0, GetResString(IDS_QL_USERNAME),		LVCFMT_LEFT,	DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_CL_UPLOADSTATUS),	LVCFMT_LEFT,	100);
	InsertColumn(2, GetResString(IDS_CL_TRANSFUP),		LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_CL_DOWNLSTATUS),	LVCFMT_LEFT,	100);
	InsertColumn(4, GetResString(IDS_CL_TRANSFDOWN),	LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(5, GetResString(IDS_CD_CSOFT),			LVCFMT_LEFT,	DFLT_CLIENTSOFT_COL_WIDTH);
	InsertColumn(6, GetResString(IDS_CONNECTED),		LVCFMT_LEFT,	50);
	CString coltemp;
	coltemp = GetResString(IDS_CD_UHASH);
	coltemp.Remove(_T(':'));
	InsertColumn(7, coltemp, LVCFMT_LEFT, DFLT_HASH_COL_WIDTH);

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100));
}

void CClientListCtrl::Localize()
{
	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;

	static const UINT uids[7] =
	{
		IDS_QL_USERNAME, IDS_CL_UPLOADSTATUS, IDS_CL_TRANSFUP, IDS_CL_DOWNLSTATUS
		, IDS_CL_TRANSFDOWN, IDS_CD_CSOFT, IDS_CONNECTED
	};

	for (int i = 0; i < _countof(uids); ++i) {
		const CString &strRes(GetResString(uids[i]));
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(i, &hdi);
	}

	CString strRes(GetResString(IDS_CD_UHASH));
	strRes.Remove(_T(':'));
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(7, &hdi);
}

void CClientListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CClientListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_pImageList = theApp.emuledlg->transferwnd->GetClientIconList();
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CClientListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	CRect rcItem(lpDrawItemStruct->rcItem);
	RECT rcClient;
	GetClientRect(&rcClient);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(lpDrawItemStruct->itemData);

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	rcItem.right = rcItem.left - sm_iLabelOffset;
	rcItem.left += sm_iIconOffset;
	for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
		int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
		if (!IsColumnHidden(iColumn)) {
			UINT uDrawTextAlignment;
			int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
			rcItem.right += iColumnWidth;
			if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem)) {
				const CString &sItem(GetItemDisplayText(client, iColumn));
				switch (iColumn) {
				case 0: //user name
					{
						int iImage;
						UINT uOverlayImage;
						client->GetDisplayImage(iImage, uOverlayImage);

						int iIconPosY = (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 1;
						const POINT point = {rcItem.left, rcItem.top + iIconPosY};
						m_pImageList->Draw(dc, iImage, point, ILD_NORMAL | INDEXTOOVERLAYMASK(uOverlayImage));

						rcItem.left += 16 + sm_iLabelOffset;
						dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
						rcItem.left -= 16;
						rcItem.right -= sm_iSubItemInset;
					}
					break;
				default:
					dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
				}
			}
			rcItem.left += iColumnWidth;
		}
	}

	DrawFocusRect(dc, &lpDrawItemStruct->rcItem, lpDrawItemStruct->itemState & ODS_FOCUS, bCtrlFocused, lpDrawItemStruct->itemState & ODS_SELECTED);
}

CString CClientListCtrl::GetItemDisplayText(const CUpDownClient *client, int iSubItem) const
{
	CString sText;
	switch (iSubItem) {
	case 0: //user name
		if (client->GetUserName() != NULL)
			sText = client->GetUserName();
		else
			sText.Format(_T("(%s)"), (LPCTSTR)GetResString(IDS_UNKNOWN));
		break;
	case 1: //upload status
		sText = client->GetUploadStateDisplayString();
		break;
	case 2: //transferred up
		if (client->credits != NULL)
			sText = CastItoXBytes(client->credits->GetUploadedTotal());
		break;
	case 3: //download status
		sText = client->GetDownloadStateDisplayString();
		break;
	case 4: //transferred down
		if (client->credits != NULL)
			sText = CastItoXBytes(client->credits->GetDownloadedTotal());
		break;
	case 5: //software
		if (client->GetClientSoftVer().IsEmpty())
			sText = GetResString(IDS_UNKNOWN);
		else
			sText = client->GetClientSoftVer();
		break;
	case 6: //connected
		sText = GetResString((client->socket && client->socket->IsConnected()) ? IDS_YES : IDS_NO);
		break;
	case 7: //hash
		sText = md4str(client->GetUserHash());
	}
	return sText;
}

void CClientListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
{
	if (!theApp.IsClosing()) {
		// Although we have an owner drawn listview control we store the text for the primary item in the listview, to be
		// capable of quick searching those items via the keyboard. Because our listview items may change their contents,
		// we do this via a text callback function. The listview control will send us the LVN_DISPINFO notification if
		// it needs to know the contents of the primary item.
		//
		// But, the listview control sends this notification all the time, even if we do not search for an item. At least
		// this notification is only sent for the visible items and not for all items in the list. Though, because this
		// function is invoked *very* often, do *NOT* put any time consuming code in here.
		//
		// Vista: That callback is used to get the strings for the label tips for the sub(!) items.
		//
		const LVITEMW &rItem = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item;
		if (rItem.mask & LVIF_TEXT) {
			const CUpDownClient *pClient = reinterpret_cast<CUpDownClient*>(rItem.lParam);
			if (pClient != NULL)
				_tcsncpy_s(rItem.pszText, rItem.cchTextMax, GetItemDisplayText(pClient, rItem.iSubItem), _TRUNCATE);
		}
	}
	*pResult = 0;
}

void CClientListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() != pNMListView->iSubItem) {
		switch (pNMListView->iSubItem) {
		case 1: // Upload State
		case 2: // Uploaded Total
		case 4: // Downloaded Total
		case 5: // Client Software
		case 6: // Connected
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}
	} else
		sortAscending = !GetSortAscending();

	// Sort table
	UpdateSortHistory(pNMListView->iSubItem + (sortAscending ? 0 : 100));
	SetSortArrow(pNMListView->iSubItem, sortAscending);
	SortItems(SortProc, pNMListView->iSubItem + (sortAscending ? 0 : 100));

	*pResult = 0;
}

int CALLBACK CClientListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CUpDownClient *item1 = reinterpret_cast<CUpDownClient*>(lParam1);
	const CUpDownClient *item2 = reinterpret_cast<CUpDownClient*>(lParam2);
	LONG_PTR iColumn = (lParamSort >= 100) ? lParamSort - 100 : lParamSort;
	int iResult = 0;
	switch (iColumn) {
	case 0: //user name
		if (item1->GetUserName() && item2->GetUserName())
			iResult = CompareLocaleStringNoCase(item1->GetUserName(), item2->GetUserName());
		else if (item1->GetUserName() == NULL)
			iResult = 1; // place clients with no user names at bottom
		else if (item2->GetUserName() == NULL)
			iResult = -1; // place clients with no user names at bottom
		break;

	case 1: //upload status
		iResult = item1->GetUploadState() - item2->GetUploadState();
		break;

	case 2: //transferred up
		if (item1->credits && item2->credits)
			iResult = CompareUnsigned64(item1->credits->GetUploadedTotal(), item2->credits->GetUploadedTotal());
		else if (item1->credits)
			iResult = 1;
		else
			iResult = -1;
		break;

	case 3: //download status
		if (item1->GetDownloadState() == item2->GetDownloadState()) {
			if (item1->IsRemoteQueueFull() && item2->IsRemoteQueueFull())
				iResult = 0;
			else if (item1->IsRemoteQueueFull())
				iResult = 1;
			else if (item2->IsRemoteQueueFull())
				iResult = -1;
		} else
			iResult = item1->GetDownloadState() - item2->GetDownloadState();
		break;

	case 4: //transferred down
		if (item1->credits && item2->credits)
			iResult = CompareUnsigned64(item1->credits->GetDownloadedTotal(), item2->credits->GetDownloadedTotal());
		else if (item1->credits)
			iResult = 1;
		else
			iResult = -1;
		break;

	case 5: //software
		if (item1->GetClientSoft() == item2->GetClientSoft())
			iResult = item1->GetVersion() - item2->GetVersion();
		else
			iResult = -(item1->GetClientSoft() - item2->GetClientSoft()); // invert result to place eMule's at top
		break;

	case 6: //connected
		if (item1->socket && item2->socket)
			iResult = item1->socket->IsConnected() - item2->socket->IsConnected();
		else if (item1->socket)
			iResult = 1;
		else
			iResult = -1;
		break;

	case 7: //hash
		iResult = memcmp(item1->GetUserHash(), item2->GetUserHash(), 16);
	}

	if (lParamSort >= 100)
		iResult = -iResult;

	//call secondary sort order, if this one results in equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->GetClientList()->GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
	}

	return iResult;
}

void CClientListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(iSel));
		if (client) {
			CClientDetailDialog dialog(client, this);
			dialog.DoModal();
		}
	}
	*pResult = 0;
}

void CClientListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	const CUpDownClient *client = (iSel >= 0) ? reinterpret_cast<CUpDownClient*>(GetItemData(iSel)) : NULL;
	const bool is_ed2k = client && client->IsEd2kClient();

	CTitleMenu ClientMenu;
	ClientMenu.CreatePopupMenu();
	ClientMenu.AddMenuTitle(GetResString(IDS_CLIENTS), true);
	ClientMenu.AppendMenu(MF_STRING | (client ? MF_ENABLED : MF_GRAYED), MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("CLIENTDETAILS"));
	ClientMenu.SetDefaultItem(MP_DETAIL);
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && !client->IsFriend()) ? MF_ENABLED : MF_GRAYED), MP_ADDFRIEND, GetResString(IDS_ADDFRIEND), _T("ADDFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | (is_ed2k ? MF_ENABLED : MF_GRAYED), MP_MESSAGE, GetResString(IDS_SEND_MSG), _T("SENDMESSAGE"));
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetViewSharedFilesSupport()) ? MF_ENABLED : MF_GRAYED), MP_SHOWLIST, GetResString(IDS_VIEWFILES), _T("VIEWFILES"));
	if (Kademlia::CKademlia::IsRunning() && !Kademlia::CKademlia::IsConnected())
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a) ? MF_ENABLED : MF_GRAYED), MP_BOOT, GetResString(IDS_BOOTSTRAP));
	ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));
	GetPopupMenuPos(*this, point);
	ClientMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

BOOL CClientListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	if (wParam == MP_FIND) {
		OnFindStart();
		return TRUE;
	}

	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(iSel));
		switch (wParam) {
		case MP_SHOWLIST:
			client->RequestSharedFileList();
			break;
		case MP_MESSAGE:
			theApp.emuledlg->chatwnd->StartSession(client);
			break;
		case MP_ADDFRIEND:
			if (theApp.friendlist->AddFriend(client))
				Update(iSel);
			break;
		case MP_UNBAN:
			if (client->IsBanned()) {
				client->UnBan();
				Update(iSel);
			}
			break;
		case MP_DETAIL:
		case MPG_ALTENTER:
		case IDA_ENTER:
			{
				CClientDetailDialog dialog(client, this);
				dialog.DoModal();
			}
			break;
		case MP_BOOT:
			if (client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a)
				Kademlia::CKademlia::Bootstrap(ntohl(client->GetIP()), client->GetKadPort());
		}
	}
	return true;
}

void CClientListCtrl::AddClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing() && !thePrefs.IsKnownClientListDisabled()) {
		int iItemCount = GetItemCount();
		InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)client);
		theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Clients, iItemCount + 1);
	}
}

void CClientListCtrl::RemoveClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing()) {
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			DeleteItem(iItem);
			theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Clients);
		}
	}
}

void CClientListCtrl::RefreshClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing()
		&& theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
		&& theApp.emuledlg->transferwnd->GetClientList()->IsWindowVisible())
	{
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			Update(iItem);
	}
}

void CClientListCtrl::ShowSelectedUserDetails()
{
	CPoint point;
	if (!::GetCursorPos(&point))
		return;
	ScreenToClient(&point);
	int it = HitTest(point);
	if (it == -1)
		return;

	SetItemState(-1, 0, LVIS_SELECTED);
	SetItemState(it, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	SetSelectionMark(it);   // display selection mark correctly!

	CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(GetSelectionMark()));
	if (client) {
		CClientDetailDialog dialog(client, this);
		dialog.DoModal();
	}
}

void CClientListCtrl::ShowKnownClients()
{
	DeleteAllItems();
	int iItemCount = 0;
	for (POSITION pos = theApp.clientlist->list.GetHeadPosition(); pos != NULL; ) {
		int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)theApp.clientlist->list.GetNext(pos));
		Update(iItem);
		++iItemCount;
	}
	theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Clients, iItemCount);
}