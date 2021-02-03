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
#include "UploadListCtrl.h"
#include "TransferWnd.h"
#include "TransferDlg.h"
#include "otherfunctions.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "KademliaWnd.h"
#include "emuledlg.h"
#include "friendlist.h"
#include "MemDC.h"
#include "KnownFile.h"
#include "SharedFileList.h"
#include "UpDownClient.h"
#include "ClientCredits.h"
#include "ChatWnd.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "UploadQueue.h"
#include "ToolTipCtrlX.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CUploadListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CUploadListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, OnLvnGetInfoTip)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CUploadListCtrl::CUploadListCtrl()
	: CListCtrlItemWalk(this)
{
	m_tooltip = new CToolTipCtrlX;
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("UploadsLv"));
}

CUploadListCtrl::~CUploadListCtrl()
{
	delete m_tooltip;
}

void CUploadListCtrl::Init()
{
	SetPrefsKey(_T("UploadListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip) {
		m_tooltip->SubclassWindow(tooltip->m_hWnd);
		tooltip->ModifyStyle(0, TTS_NOPREFIX);
		tooltip->SetDelayTime(TTDT_AUTOPOP, 20000);
		tooltip->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	}

	InsertColumn(0, GetResString(IDS_QL_USERNAME),	LVCFMT_LEFT, DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_FILE),			LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_DL_SPEED),		LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_DL_TRANSF),	LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);
	InsertColumn(4, GetResString(IDS_WAITED),		LVCFMT_LEFT, 60);
	InsertColumn(5, GetResString(IDS_UPLOADTIME),	LVCFMT_LEFT, 80);
	InsertColumn(6, GetResString(IDS_STATUS),		LVCFMT_LEFT, 100);
	InsertColumn(7, GetResString(IDS_UPSTATUS),		LVCFMT_LEFT, DFLT_PARTSTATUS_COL_WIDTH);

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100));
}

void CUploadListCtrl::Localize()
{
	static const UINT uids[8] =
	{
		IDS_QL_USERNAME, IDS_FILE, IDS_DL_SPEED, IDS_DL_TRANSF, IDS_WAITED
		, IDS_UPLOADTIME, IDS_STATUS, IDS_UPSTATUS
	};

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;

	for (int i = 0; i < _countof(uids); ++i) {
		CString strRes(GetResString(uids[i]));
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(i, &hdi);
	}
}

void CUploadListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CUploadListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_pImageList = theApp.emuledlg->transferwnd->GetClientIconList();
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CUploadListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	CRect rcItem(lpDrawItemStruct->rcItem);
	CRect rcClient;
	GetClientRect(&rcClient);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(lpDrawItemStruct->itemData);
	if (client->GetSlotNumber() > (UINT)theApp.uploadqueue->GetActiveUploadsCount())
		dc.SetTextColor(::GetSysColor(COLOR_GRAYTEXT));

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
				case 0:
					{
						int iImage;
						UINT uOverlayImage;
						client->GetDisplayImage(iImage, uOverlayImage);

						int iIconPosY = (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 1;
						POINT point = {rcItem.left, rcItem.top + iIconPosY};
						m_pImageList->Draw(dc, iImage, point, ILD_NORMAL | INDEXTOOVERLAYMASK(uOverlayImage));

						rcItem.left += 16 + sm_iLabelOffset;
						dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
						rcItem.left -= 16;
						rcItem.right -= sm_iSubItemInset;
					}
					break;
				case 7:
					++rcItem.top;
					--rcItem.bottom;
					client->DrawUpStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
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

CString  CUploadListCtrl::GetItemDisplayText(const CUpDownClient *client, int iSubItem) const
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
		{
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file)
				sText = file->GetFileName();
		}
		break;
	case 2:
		sText = CastItoXBytes(client->GetDatarate(), false, true);
		break;
	case 3:
		// NOTE: If you change (add/remove) anything which is displayed here, update also the sorting part.
		if (!thePrefs.m_bExtControls)
			return CastItoXBytes(client->GetSessionUp());
		sText.Format(_T("%s (%s)"), (LPCTSTR)CastItoXBytes(client->GetSessionUp()), (LPCTSTR)CastItoXBytes(client->GetQueueSessionPayloadUp()));
		break;
	case 4:
		if (!client->HasLowID())
			sText = CastSecondsToHM(client->GetWaitTime() / SEC2MS(1));
		else
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastSecondsToHM(client->GetWaitTime() / SEC2MS(1)), (LPCTSTR)GetResString(IDS_IDLOW));
		break;
	case 5:
		sText = CastSecondsToHM(client->GetUpStartTimeDelay() / SEC2MS(1));
		break;
	case 6:
		sText = client->GetUploadStateDisplayString();
		break;
	case 7:
		sText = GetResString(IDS_UPSTATUS);
	}
	return sText;
}

void CUploadListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
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

void CUploadListCtrl::OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);
	LVHITTESTINFO hti;
	if (pGetInfoTip->iSubItem == 0 && ::GetCursorPos(&hti.pt)) {
		ScreenToClient(&hti.pt);
		if (SubItemHitTest(&hti) == -1 || hti.iItem != pGetInfoTip->iItem || hti.iSubItem != 0) {
			// don't show the default label tip for the main item, if the mouse is not over the main item
			if ((pGetInfoTip->dwFlags & LVGIT_UNFOLDED) == 0 && pGetInfoTip->cchTextMax > 0 && pGetInfoTip->pszText)
				pGetInfoTip->pszText[0] = _T('\0');
			return;
		}

		const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(pGetInfoTip->iItem));
		if (client && pGetInfoTip->pszText && pGetInfoTip->cchTextMax > 0) {
			CString strInfo;
			strInfo.Format(GetResString(IDS_USERINFO), client->GetUserName());
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file) {
				strInfo.AppendFormat(_T("%s %s\n"), (LPCTSTR)GetResString(IDS_SF_REQUESTED), (LPCTSTR)file->GetFileName());
				strInfo.AppendFormat(GetResString(IDS_FILESTATS_SESSION) + GetResString(IDS_FILESTATS_TOTAL),
					file->statistic.GetAccepts(), file->statistic.GetRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetTransferred()),
					file->statistic.GetAllTimeAccepts(), file->statistic.GetAllTimeRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetAllTimeTransferred()));
			} else
				strInfo += GetResString(IDS_REQ_UNKNOWNFILE);

			strInfo += TOOLTIP_AUTOFORMAT_SUFFIX_CH;
			_tcsncpy(pGetInfoTip->pszText, strInfo, pGetInfoTip->cchTextMax);
			pGetInfoTip->pszText[pGetInfoTip->cchTextMax - 1] = _T('\0');
		}
	}
	*pResult = 0;
}

void CUploadListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() != pNMListView->iSubItem) {
		switch (pNMListView->iSubItem) {
		case 2: // Data rate
		case 3: // Session Up
		case 4: // Wait Time
		case 7: // Part Count
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

int CALLBACK CUploadListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CUpDownClient *item1 = reinterpret_cast<CUpDownClient*>(lParam1);
	const CUpDownClient *item2 = reinterpret_cast<CUpDownClient*>(lParam2);
	LPARAM iColumn = (lParamSort >= 100) ? lParamSort - 100 : lParamSort;
	int iResult = 0;
	switch (iColumn) {
	case 0:
		if (item1->GetUserName() && item2->GetUserName())
			iResult = CompareLocaleStringNoCase(item1->GetUserName(), item2->GetUserName());
		else if (item1->GetUserName() == NULL)
			iResult = 1; // place clients with no usernames at bottom
		else if (item2->GetUserName() == NULL)
			iResult = -1; // place clients with no usernames at bottom
		break;
	case 1:
		{
			const CKnownFile *file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
			const CKnownFile *file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
			if (file1 != NULL && file2 != NULL)
				iResult = CompareLocaleStringNoCase(file1->GetFileName(), file2->GetFileName());
			else
				iResult = (file1 == NULL) ? 1 : -1;
		}
		break;
	case 2:
		iResult = CompareUnsigned(item1->GetDatarate(), item2->GetDatarate());
		break;
	case 3:
		iResult = CompareUnsigned(item1->GetSessionUp(), item2->GetSessionUp());
		if (iResult == 0 && thePrefs.m_bExtControls)
			iResult = CompareUnsigned(item1->GetQueueSessionPayloadUp(), item2->GetQueueSessionPayloadUp());
		break;
	case 4:
		iResult = CompareUnsigned(item1->GetWaitTime(), item2->GetWaitTime());
		break;
	case 5:
		iResult = CompareUnsigned(item1->GetUpStartTimeDelay(), item2->GetUpStartTimeDelay());
		break;
	case 6:
		iResult = item1->GetUploadState() - item2->GetUploadState();
		break;
	case 7:
		iResult = CompareUnsigned(item1->GetUpPartCount(), item2->GetUpPartCount());
	}

	if (lParamSort >= 100)
		iResult = -iResult;

	//call secondary sortorder, if this one results in equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
	}

	return iResult;
}

void CUploadListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
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

void CUploadListCtrl::OnContextMenu(CWnd*, CPoint point)
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

BOOL CUploadListCtrl::OnCommand(WPARAM wParam, LPARAM)
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

void CUploadListCtrl::AddClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	int iItemCount = GetItemCount();
	int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)client);
	Update(iItem);
	theApp.emuledlg->transferwnd->m_pwndTransfer->UpdateListCount(CTransferWnd::wnd2Uploading, iItemCount + 1);
}

void CUploadListCtrl::RemoveClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)client;
	int iItem = FindItem(&find);
	if (iItem >= 0) {
		DeleteItem(iItem);
		theApp.emuledlg->transferwnd->m_pwndTransfer->UpdateListCount(CTransferWnd::wnd2Uploading);
	}
}

void CUploadListCtrl::RefreshClient(const CUpDownClient *client)
{
	if (theApp.IsClosing()
		|| theApp.emuledlg->activewnd != theApp.emuledlg->transferwnd
		|| !theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.IsWindowVisible())
	{
		return;
	}

	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)client;
	int iItem = FindItem(&find);
	if (iItem >= 0)
		Update(iItem);
}

void CUploadListCtrl::ShowSelectedUserDetails()
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