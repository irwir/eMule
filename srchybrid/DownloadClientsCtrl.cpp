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
#include "emuledlg.h"
#include "DownloadClientsCtrl.h"
#include "ClientDetailDialog.h"
#include "MemDC.h"
#include "MenuCmds.h"
#include "FriendList.h"
#include "TransferDlg.h"
#include "ChatWnd.h"
#include "UpDownClient.h"
#include "UploadQueue.h"
#include "ClientCredits.h"
#include "PartFile.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "SharedFileList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CDownloadClientsCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CDownloadClientsCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CDownloadClientsCtrl::CDownloadClientsCtrl()
	: CListCtrlItemWalk(this)
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("DownloadingLv"));
}

void CDownloadClientsCtrl::Init()
{
	SetPrefsKey(_T("DownloadClientsCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(0, GetResString(IDS_QL_USERNAME),	LVCFMT_LEFT,	DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_CD_CSOFT),		LVCFMT_LEFT,	DFLT_CLIENTSOFT_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_FILE),			LVCFMT_LEFT,	DFLT_FILENAME_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_DL_SPEED),		LVCFMT_RIGHT,	DFLT_DATARATE_COL_WIDTH);
	InsertColumn(4, GetResString(IDS_AVAILABLEPARTS), LVCFMT_LEFT,	DFLT_PARTSTATUS_COL_WIDTH);
	InsertColumn(5, GetResString(IDS_CL_TRANSFDOWN), LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(6, GetResString(IDS_CL_TRANSFUP),	LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(7, GetResString(IDS_META_SRCTYPE),	LVCFMT_LEFT,	100);

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100));
}

void CDownloadClientsCtrl::Localize()
{
	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;

	CString strRes(GetResString(IDS_QL_USERNAME));
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(0, &hdi);

	strRes = GetResString(IDS_CD_CSOFT);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(1, &hdi);

	strRes = GetResString(IDS_FILE);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(2, &hdi);

	strRes = GetResString(IDS_DL_SPEED);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(3, &hdi);

	strRes = GetResString(IDS_AVAILABLEPARTS);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(4, &hdi);

	strRes = GetResString(IDS_CL_TRANSFDOWN);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(5, &hdi);

	strRes = GetResString(IDS_CL_TRANSFUP);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(6, &hdi);

	strRes = GetResString(IDS_META_SRCTYPE);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(7, &hdi);
}

void CDownloadClientsCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CDownloadClientsCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_pImageList = theApp.emuledlg->transferwnd->GetClientIconList();
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CDownloadClientsCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	CRect rcClient;
	GetClientRect(rcClient);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(lpDrawItemStruct->itemData);

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	CRect rcItem(lpDrawItemStruct->rcItem);
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
				case 0:
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
				case 4:
					++rcItem.top;
					--rcItem.bottom;
					client->DrawStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
					++rcItem.bottom;
					--rcItem.top;
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

CString CDownloadClientsCtrl::GetItemDisplayText(const CUpDownClient *client, int iSubItem) const
{
	CString sText;
	switch (iSubItem) {
	case 0:
		if (client->GetUserName() != NULL)
			sText = client->GetUserName();
		else
			sText.Format(_T("(%s)"), (LPCTSTR)GetResString(IDS_UNKNOWN));
		break;
	case 1:
		sText = client->GetClientSoftVer();
		break;
	case 2:
		sText = client->GetRequestFile()->GetFileName();
		break;
	case 3:
		sText = CastItoXBytes((float)client->GetDownloadDatarate(), false, true);
		break;
	case 4:
		sText = GetResString(IDS_AVAILABLEPARTS);
		break;
	case 5:
		if (client->credits == NULL || client->GetSessionDown() >= client->credits->GetDownloadedTotal())
			sText = CastItoXBytes(client->GetSessionDown());
		else
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastItoXBytes(client->GetSessionDown()), (LPCTSTR)CastItoXBytes(client->credits->GetDownloadedTotal()));
		break;
	case 6:
		if (client->credits == NULL || client->GetSessionUp() >= client->credits->GetUploadedTotal())
			sText = CastItoXBytes(client->GetSessionUp());
		else
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastItoXBytes(client->GetSessionUp()), (LPCTSTR)CastItoXBytes(client->credits->GetUploadedTotal()));
		break;
	case 7:
		{
			UINT uid;
			switch (client->GetSourceFrom()) {
			case SF_SERVER:
				uid = IDS_ED2KSERVER;
				break;
			case SF_KADEMLIA:
				uid = IDS_KADEMLIA;
				break;
			case SF_SOURCE_EXCHANGE:
				uid = IDS_SE;
				break;
			case SF_PASSIVE:
				uid = IDS_PASSIVE;
				break;
			case SF_LINK:
				uid = IDS_SW_LINK;
				break;
			default:
				uid = IDS_UNKNOWN;
			}
			sText = GetResString(uid);
		}
	}
	return sText;
}


void CDownloadClientsCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
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

void CDownloadClientsCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() == pNMListView->iSubItem)
		sortAscending = !GetSortAscending();
	else
		switch (pNMListView->iSubItem) {
		case 1: // Client Software
		case 3: // Download Rate
		case 4: // Part Count
		case 5: // Session Down
		case 6: // Session Up
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}

	// Sort table
	UpdateSortHistory(pNMListView->iSubItem + (sortAscending ? 0 : 100));
	SetSortArrow(pNMListView->iSubItem, sortAscending);
	SortItems(SortProc, pNMListView->iSubItem + (sortAscending ? 0 : 100));

	*pResult = 0;
}

int CALLBACK CDownloadClientsCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CUpDownClient *item1 = reinterpret_cast<CUpDownClient*>(lParam1);
	const CUpDownClient *item2 = reinterpret_cast<CUpDownClient*>(lParam2);
	LPARAM iColumn = (lParamSort >= 100) ? lParamSort - 100 : lParamSort;
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
	case 1: //version
		if (item1->GetClientSoft() == item2->GetClientSoft())
			iResult = item1->GetVersion() - item2->GetVersion();
		else
			iResult = -(item1->GetClientSoft() - item2->GetClientSoft()); // invert result to place eMule's at top
		break;
	case 2:
		{
			const CKnownFile *file1 = item1->GetRequestFile();
			const CKnownFile *file2 = item2->GetRequestFile();
			if ((file1 != NULL) && (file2 != NULL))
				iResult = CompareLocaleStringNoCase(file1->GetFileName(), file2->GetFileName());
			else if (file1 == NULL)
				iResult = 1;
			else
				iResult = -1;
		}
		break;
	case 3: //download rate
		iResult = CompareUnsigned(item1->GetDownloadDatarate(), item2->GetDownloadDatarate());
		break;
	case 4: //part count
		iResult = CompareUnsigned(item1->GetPartCount(), item2->GetPartCount());
		break;
	case 5: //download sessions
		iResult = CompareUnsigned(item1->GetSessionDown(), item2->GetSessionDown());
		break;
	case 6:
		iResult = CompareUnsigned(item1->GetSessionUp(), item2->GetSessionUp());
		break;
	case 7:
		iResult = item1->GetSourceFrom() - item2->GetSourceFrom();
	}

	if (lParamSort >= 100)
		iResult = -iResult;

	//call secondary sort order, if this one's results is equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->GetDownloadClientsList()->GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
	}

	return iResult;
}

void CDownloadClientsCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
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

void CDownloadClientsCtrl::OnContextMenu(CWnd*, CPoint point)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(iSel >= 0 ? GetItemData(iSel) : NULL);
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

BOOL CDownloadClientsCtrl::OnCommand(WPARAM wParam, LPARAM)
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

void CDownloadClientsCtrl::AddClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	int iItemCount = GetItemCount();
	InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)client);
	theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Downloading, iItemCount + 1);
}

void CDownloadClientsCtrl::RemoveClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)client;
	int iItem = FindItem(&find);
	if (iItem >= 0) {
		DeleteItem(iItem);
		theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2Downloading, GetItemCount());
	}
}

void CDownloadClientsCtrl::RefreshClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing()
		&& theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
		&& theApp.emuledlg->transferwnd->GetDownloadClientsList()->IsWindowVisible())
	{
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			Update(iItem);
	}
}

void CDownloadClientsCtrl::ShowSelectedUserDetails()
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