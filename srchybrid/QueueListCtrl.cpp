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
#include "QueueListCtrl.h"
#include "OtherFunctions.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "Exceptions.h"
#include "KademliaWnd.h"
#include "emuledlg.h"
#include "FriendList.h"
#include "UploadQueue.h"
#include "UpDownClient.h"
#include "TransferDlg.h"
#include "MemDC.h"
#include "SharedFileList.h"
#include "ClientCredits.h"
#include "PartFile.h"
#include "ChatWnd.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/Kademlia/Prefs.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CQueueListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CQueueListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CQueueListCtrl::CQueueListCtrl()
	: CListCtrlItemWalk(this)
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("QueuedLv"));

	// Barry - Refresh the queue every 10 secs
	VERIFY((m_hTimer = ::SetTimer(NULL, 0, SEC2MS(10), QueueUpdateTimer)) != 0);
	if (thePrefs.GetVerbose() && !m_hTimer)
		AddDebugLogLine(true, _T("Failed to create 'queue list control' timer - %s"), (LPCTSTR)GetErrorMessage(::GetLastError()));
}

CQueueListCtrl::~CQueueListCtrl()
{
	if (m_hTimer)
		VERIFY(::KillTimer(NULL, m_hTimer));
}

void CQueueListCtrl::Init()
{
	SetPrefsKey(_T("QueueListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(0, GetResString(IDS_QL_USERNAME),	LVCFMT_LEFT, DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_FILE),			LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_FILEPRIO),		LVCFMT_LEFT, DFLT_PRIORITY_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_QL_RATING),	LVCFMT_LEFT,  60);
	InsertColumn(4, GetResString(IDS_SCORE),		LVCFMT_LEFT,  60);
	InsertColumn(5, GetResString(IDS_ASKED),		LVCFMT_LEFT,  60);
	InsertColumn(6, GetResString(IDS_LASTSEEN),		LVCFMT_LEFT, 110);
	InsertColumn(7, GetResString(IDS_ENTERQUEUE),	LVCFMT_LEFT, 110);
	InsertColumn(8, GetResString(IDS_BANNED),		LVCFMT_LEFT,  60);
	InsertColumn(9, GetResString(IDS_UPSTATUS),		LVCFMT_LEFT, DFLT_PARTSTATUS_COL_WIDTH);

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100));
}

void CQueueListCtrl::Localize()
{
	static const UINT uids[10] =
	{
		IDS_QL_USERNAME, IDS_FILE, IDS_FILEPRIO, IDS_QL_RATING, IDS_SCORE
		, IDS_ASKED, IDS_LASTSEEN, IDS_ENTERQUEUE, IDS_BANNED, IDS_UPSTATUS
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

void CQueueListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CQueueListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_pImageList = theApp.emuledlg->transferwnd->GetClientIconList();
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CQueueListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
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
				case 9: //obtained parts
					if (client->GetUpPartCount()) {
						++rcItem.top;
						--rcItem.bottom;
						client->DrawUpStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
						++rcItem.bottom;
						--rcItem.top;
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

CString CQueueListCtrl::GetItemDisplayText(const CUpDownClient *client, int iSubItem) const
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
		{
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file) {
				UINT uid;
				switch (file->GetUpPriority()) {
				case PR_VERYLOW:
					uid = IDS_PRIOVERYLOW;
					break;
				case PR_LOW:
					uid = file->IsAutoUpPriority() ? IDS_PRIOAUTOLOW : IDS_PRIOLOW;
					break;
				case PR_NORMAL:
					uid = file->IsAutoUpPriority() ? IDS_PRIOAUTONORMAL : IDS_PRIONORMAL;
					break;
				case PR_HIGH:
					uid = file->IsAutoUpPriority() ? IDS_PRIOAUTOHIGH : IDS_PRIOHIGH;
					break;
				case PR_VERYHIGH:
					uid = IDS_PRIORELEASE;
					break;
				default:
					uid = 0;
				}
				if (uid)
					sText = GetResString(uid);
			}
		}
		break;
	case 3:
		sText.Format(_T("%u"), client->GetScore(false, false, true));
		break;
	case 4:
		{
			UINT uScore = client->GetScore(false);
			if (client->HasLowID()) {
				if (client->m_bAddNextConnect)
					sText.Format(_T("%u ****"), uScore);
				else
					sText.Format(_T("%u (%s)"), uScore, (LPCTSTR)GetResString(IDS_IDLOW));
			} else
				sText.Format(_T("%u"), uScore);
		}
		break;
	case 5:
		sText.Format(_T("%u"), client->GetAskedCount());
		break;
	case 6:
		sText = CastSecondsToHM((::GetTickCount() - client->GetLastUpRequest()) / SEC2MS(1));
		break;
	case 7:
		sText = CastSecondsToHM((::GetTickCount() - client->GetWaitStartTime()) / SEC2MS(1));
		break;
	case 8:
		sText = GetResString(client->IsBanned() ? IDS_YES : IDS_NO);
		break;
	case 9:
		sText = GetResString(IDS_UPSTATUS);
	}
	return sText;
}

void CQueueListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
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

void CQueueListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() == pNMListView->iSubItem)
		sortAscending = !GetSortAscending();
	else
		switch (pNMListView->iSubItem) {
		case 2: // Up Priority
		case 3: // Rating
		case 4: // Score
		case 5: // Ask Count
		case 8: // Banned
		case 9: // Part Count
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

int CALLBACK CQueueListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
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
			iResult = 1; // place clients with no user names at bottom
		else if (item2->GetUserName() == NULL)
			iResult = -1; // place clients with no user names at bottom
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
		{
			const CKnownFile *file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
			const CKnownFile *file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
			if (file1 != NULL && file2 != NULL)
				iResult = (file1->GetUpPriority() == PR_VERYLOW ? -1 : file1->GetUpPriority()) - (file2->GetUpPriority() == PR_VERYLOW ? -1 : file2->GetUpPriority());
			else
				iResult = (file1 == NULL) ? 1 : -1;

		}
		break;
	case 3:
		iResult = CompareUnsigned(item1->GetScore(false, false, true), item2->GetScore(false, false, true));
		break;
	case 4:
		iResult = CompareUnsigned(item1->GetScore(false), item2->GetScore(false));
		break;
	case 5:
		iResult = CompareUnsigned(item1->GetAskedCount(), item2->GetAskedCount());
		break;
	case 6:
		iResult = CompareUnsigned(item1->GetLastUpRequest(), item2->GetLastUpRequest());
		break;
	case 7:
		iResult = CompareUnsigned(item1->GetWaitStartTime(), item2->GetWaitStartTime());
		break;
	case 8:
		iResult = item1->IsBanned() - item2->IsBanned();
		break;
	case 9:
		iResult = CompareUnsigned(item1->GetUpPartCount(), item2->GetUpPartCount());
	}

	if (lParamSort >= 100)
		iResult = -iResult;

	//call secondary sortorder, if this one results in equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->GetQueueList()->GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
	}

	return iResult;
}

void CQueueListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
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

void CQueueListCtrl::OnContextMenu(CWnd*, CPoint point)
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
	if (thePrefs.IsExtControlsEnabled())
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->IsBanned()) ? MF_ENABLED : MF_GRAYED), MP_UNBAN, GetResString(IDS_UNBAN));
	if (Kademlia::CKademlia::IsRunning() && !Kademlia::CKademlia::IsConnected())
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetKadPort() != 0 && client->GetKadVersion() >= KADEMLIA_VERSION2_47a) ? MF_ENABLED : MF_GRAYED), MP_BOOT, GetResString(IDS_BOOTSTRAP));
	ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));
	GetPopupMenuPos(*this, point);
	ClientMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

BOOL CQueueListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	switch (wParam) {
	case MP_FIND:
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

void CQueueListCtrl::AddClient(CUpDownClient *client, bool resetclient)
{
	if (resetclient && client) {
		client->SetWaitStartTime();
		client->SetAskedCount(1);
	}
	if (!theApp.IsClosing() && !thePrefs.IsQueueListDisabled()) {
		int iItemCount = GetItemCount();
		int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)client);
		Update(iItem);
		theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2OnQueue, iItemCount + 1);
	}
}

void CQueueListCtrl::RemoveClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing()) {
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			DeleteItem(iItem);
			theApp.emuledlg->transferwnd->UpdateListCount(CTransferDlg::wnd2OnQueue);
		}
	}
}

void CQueueListCtrl::RefreshClient(const CUpDownClient *client)
{
	if (!theApp.IsClosing()
		&& theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
		&& theApp.emuledlg->transferwnd->GetQueueList()->IsWindowVisible())
	{
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			Update(iItem);
	}
}

void CQueueListCtrl::ShowSelectedUserDetails()
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

void CQueueListCtrl::ShowQueueClients()
{
	DeleteAllItems();
	for (CUpDownClient *update = NULL; (update = theApp.uploadqueue->GetNextClient(update)) != NULL;)
		AddClient(update, false);
}

// Barry - Refresh the queue every 10 secs
void CALLBACK CQueueListCtrl::QueueUpdateTimer(HWND /*hwnd*/, UINT /*uiMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) noexcept
{
	// NOTE: Always handle all type of MFC exceptions in TimerProcs - otherwise we'll get mem leaks
	try {
		if (theApp.IsClosing() // Don't do anything if the app is shutting down - can cause unhandled exceptions
			|| !thePrefs.GetUpdateQueueList()
			|| theApp.emuledlg->activewnd != theApp.emuledlg->transferwnd
			|| !theApp.emuledlg->transferwnd->GetQueueList()->IsWindowVisible())
			return;

		const CUpDownClient *update = NULL;
		while ((update = theApp.uploadqueue->GetNextClient(update)) != NULL)
			theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(update);
	}
	CATCH_DFLT_EXCEPTIONS(_T("CQueueListCtrl::QueueUpdateTimer"))
}