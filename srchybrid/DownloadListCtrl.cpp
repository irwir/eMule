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
#include "DownloadListCtrl.h"
#include "otherfunctions.h"
#include "updownclient.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "FileDetailDialog.h"
#include "commentdialoglst.h"
#include "MetaDataDlg.h"
#include "InputBox.h"
#include "KademliaWnd.h"
#include "emuledlg.h"
#include "DownloadQueue.h"
#include "FriendList.h"
#include "PartFile.h"
#include "ClientCredits.h"
#include "MemDC.h"
#include "ChatWnd.h"
#include "TransferDlg.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/Kademlia/Prefs.h"
#include "Kademlia/net/KademliaUDPListener.h"
#include "WebServices.h"
#include "Preview.h"
#include "StringConversion.h"
#include "AddSourceDlg.h"
#include "ToolTipCtrlX.h"
#include "CollectionViewDialog.h"
#include "SearchDlg.h"
#include "SharedFileList.h"
#include "ToolbarWnd.h"
#include "ImportParts.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CDownloadListCtrl

#define DLC_BARUPDATE 512

#define RATING_ICON_WIDTH	16


IMPLEMENT_DYNAMIC(CtrlItem_Struct, CObject)

IMPLEMENT_DYNAMIC(CDownloadListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CDownloadListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnListModified)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, OnLvnGetInfoTip)
	ON_NOTIFY_REFLECT(LVN_INSERTITEM, OnListModified)
	ON_NOTIFY_REFLECT(LVN_ITEMACTIVATE, OnLvnItemActivate)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnListModified)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CDownloadListCtrl::CDownloadListCtrl()
	: CDownloadListListCtrlItemWalk(this)
	, curTab()
	, m_bRemainSort()
	, m_pFontBold()
	, m_dwLastAvailableCommandsCheck()
	, m_availableCommandsDirty(true)
{
	m_tooltip = new CToolTipCtrlX;
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("DownloadsLv"));
}

CDownloadListCtrl::~CDownloadListCtrl()
{
	if (m_PreviewMenu)
		VERIFY(m_PreviewMenu.DestroyMenu());
	if (m_PrioMenu)
		VERIFY(m_PrioMenu.DestroyMenu());
	if (m_SourcesMenu)
		VERIFY(m_SourcesMenu.DestroyMenu());
	if (m_FileMenu)
		VERIFY(m_FileMenu.DestroyMenu());

	while (!m_ListItems.empty()) {
		delete m_ListItems.begin()->second; // second = CtrlItem_Struct*
		m_ListItems.erase(m_ListItems.begin());
	}
	delete m_tooltip;
}

void CDownloadListCtrl::Init()
{
	SetPrefsKey(_T("DownloadListCtrl"));
	SetStyle();
	ASSERT((GetStyle() & LVS_SINGLESEL) == 0);

	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip) {
		m_tooltip->SetFileIconToolTip(true);
		m_tooltip->SubclassWindow(*tooltip);
		tooltip->ModifyStyle(0, TTS_NOPREFIX);
		tooltip->SetDelayTime(TTDT_AUTOPOP, SEC2MS(20));
		tooltip->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	}

	InsertColumn(0, GetResString(IDS_DL_FILENAME),	LVCFMT_LEFT,	DFLT_FILENAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_DL_SIZE),		LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_DL_TRANSF),	LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH, -1, true);
	InsertColumn(3, GetResString(IDS_DL_TRANSFCOMPL), LVCFMT_RIGHT,	DFLT_SIZE_COL_WIDTH);
	InsertColumn(4, GetResString(IDS_DL_SPEED),		LVCFMT_RIGHT,	DFLT_DATARATE_COL_WIDTH);
	InsertColumn(5, GetResString(IDS_DL_PROGRESS),	LVCFMT_LEFT,	DFLT_PARTSTATUS_COL_WIDTH);
	InsertColumn(6, GetResString(IDS_DL_SOURCES),	LVCFMT_RIGHT,	60);
	InsertColumn(7, GetResString(IDS_PRIORITY),		LVCFMT_LEFT,	DFLT_PRIORITY_COL_WIDTH);
	InsertColumn(8, GetResString(IDS_STATUS),		LVCFMT_LEFT,	70);
	InsertColumn(9, GetResString(IDS_DL_REMAINS),	LVCFMT_LEFT,	110);
	CString stitle(GetResString(IDS_LASTSEENCOMPL));
	stitle.Remove(_T(':'));
	InsertColumn(10, stitle,						LVCFMT_LEFT,	150, -1, true);
	stitle = GetResString(IDS_FD_LASTCHANGE);
	stitle.Remove(_T(':'));
	InsertColumn(11, stitle,						LVCFMT_LEFT,	120, -1, true);
	InsertColumn(12, GetResString(IDS_CAT),			LVCFMT_LEFT,	100, -1, true);
	InsertColumn(13, GetResString(IDS_ADDEDON),		LVCFMT_LEFT,	120);

	SetAllIcons();
	Localize();
	LoadSettings();
	curTab = 0;

	if (thePrefs.GetShowActiveDownloadsBold()) {
		if (thePrefs.GetUseSystemFontForMainControls()) {
			CFont *pFont = GetFont();
			LOGFONT lfFont;
			pFont->GetLogFont(&lfFont);
			lfFont.lfWeight = FW_BOLD;
			m_fontBold.CreateFontIndirect(&lfFont);
			m_pFontBold = &m_fontBold;
		} else
			m_pFontBold = &theApp.m_fontDefaultBold;
	}

	// Barry - Use preferred sort order from preferences
	m_bRemainSort = thePrefs.TransferlistRemainSortStyle();
	int adder;
	if (GetSortItem() != 9 || !m_bRemainSort) {
		SetSortArrow();
		adder = 0;
	} else {
		SetSortArrow(GetSortItem(), GetSortAscending() ? arrowDoubleUp : arrowDoubleDown);
		adder = 81;
	}
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100) + adder);
}

void CDownloadListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
	CreateMenus();
}

void CDownloadListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_ImageList.DeleteImageList();
	m_ImageList.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	m_ImageList.Add(CTempIconLoader(_T("SrcDownloading")));
	m_ImageList.Add(CTempIconLoader(_T("SrcOnQueue")));
	m_ImageList.Add(CTempIconLoader(_T("SrcConnecting")));
	m_ImageList.Add(CTempIconLoader(_T("SrcNNPQF")));
	m_ImageList.Add(CTempIconLoader(_T("SrcUnknown")));
	m_ImageList.Add(CTempIconLoader(_T("ClientCompatible")));
	m_ImageList.Add(CTempIconLoader(_T("Friend")));
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkey")));
	m_ImageList.Add(CTempIconLoader(_T("ClientMLDonkey")));
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkeyHybrid")));
	m_ImageList.Add(CTempIconLoader(_T("ClientShareaza")));
	m_ImageList.Add(CTempIconLoader(_T("Server")));
	m_ImageList.Add(CTempIconLoader(_T("ClientAMule")));
	m_ImageList.Add(CTempIconLoader(_T("ClientLPhant")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_NotRated")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Fake")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Poor")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Fair")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Good")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Excellent")));
	m_ImageList.Add(CTempIconLoader(_T("Collection_Search"))); // rating for comments are searched on kad
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("ClientSecureOvl"))), 1);
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("OverlayObfu"))), 2);
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("OverlaySecureObfu"))), 3);
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(m_ImageList) == NULL);
}

void CDownloadListCtrl::Localize()
{
	static const UINT uids[10] =
	{
		IDS_DL_FILENAME, IDS_DL_SIZE, IDS_DL_TRANSF, IDS_DL_TRANSFCOMPL, IDS_DL_SPEED
		, IDS_DL_PROGRESS, IDS_DL_SOURCES, IDS_PRIORITY, IDS_STATUS, IDS_DL_REMAINS
	};

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;
	CString strRes;

	for (int i = 0; i < _countof(uids); ++i) {
		strRes = GetResString(uids[i]);
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(i, &hdi);
	}

	strRes = GetResString(IDS_LASTSEENCOMPL);
	strRes.Remove(_T(':'));
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(10, &hdi);

	strRes = GetResString(IDS_FD_LASTCHANGE);
	strRes.Remove(_T(':'));
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(11, &hdi);

	strRes = GetResString(IDS_CAT);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(12, &hdi);

	strRes = GetResString(IDS_ADDEDON);
	hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
	pHeaderCtrl->SetItem(13, &hdi);

	CreateMenus();
	ShowFilesCount();
}

void CDownloadListCtrl::AddFile(CPartFile *toadd)
{
	// Create new Item
	CtrlItem_Struct *newitem = new CtrlItem_Struct;
	int itemnr = GetItemCount();
	newitem->owner = NULL;
	newitem->type = FILE_TYPE;
	newitem->value = toadd;
	newitem->parent = NULL;
	newitem->dwUpdated = 0;

	// The same file shall be added only once
	ASSERT(m_ListItems.find(toadd) == m_ListItems.end());
	m_ListItems.insert(ListItemsPair(toadd, newitem));

	if (toadd->CheckShowItemInGivenCat(curTab))
		InsertItem(LVIF_PARAM | LVIF_TEXT, itemnr, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)newitem);

	ShowFilesCount();
}

void CDownloadListCtrl::AddSource(CPartFile *owner, CUpDownClient *source, bool notavailable)
{
	// Create new Item
	CtrlItem_Struct *newitem = new CtrlItem_Struct;
	newitem->owner = owner;
	newitem->type = (notavailable ? UNAVAILABLE_SOURCE : AVAILABLE_SOURCE);
	newitem->value = source;
	newitem->dwUpdated = 0;

	// Update cross link to the owner
	ListItems::const_iterator ownerIt = m_ListItems.find(owner);
	ASSERT(ownerIt != m_ListItems.end());
	CtrlItem_Struct *ownerItem = ownerIt->second;
	ASSERT(ownerItem->value == owner);
	newitem->parent = ownerItem;

	// The same source could be added a few times but only one time per file
	
	// Update the other instances of this source
	bool bFound = false;
	for (ListItems::const_iterator it = m_ListItems.find(source); it != m_ListItems.end() && it->first == source; ++it) {
		CtrlItem_Struct *cur_item = it->second;

		// Check if this source has been already added to this file => to be sure
		if (cur_item->owner == owner) {
			// Update this instance with its new setting
			cur_item->type = newitem->type;
			cur_item->dwUpdated = 0;
			bFound = true;
		} else if (!notavailable) {
			// The state 'Available' is exclusive
			cur_item->type = UNAVAILABLE_SOURCE;
			cur_item->dwUpdated = 0;
		}
	}

	if (bFound) {
		delete newitem;
		return;
	}
	
	m_ListItems.insert(ListItemsPair(source, newitem));

	if (owner->srcarevisible) {
		// find parent from the CListCtrl to add source
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)ownerItem;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			InsertItem(LVIF_PARAM | LVIF_TEXT, iItem + 1, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)newitem);
	}
}

void CDownloadListCtrl::RemoveSource(CUpDownClient *source, CPartFile *owner)
{
	if (theApp.IsClosing())
		return;

	// Retrieve all entries matching the source
	for (ListItems::const_iterator it = m_ListItems.find(source); it != m_ListItems.end() && it->first == source;) {
		const CtrlItem_Struct *delItem = it->second;
		if (owner == NULL || owner == delItem->owner) {
			// Remove it from the m_ListItems
			it = m_ListItems.erase(it);

			// Remove it from the CListCtrl
			LVFINDINFO find;
			find.flags = LVFI_PARAM;
			find.lParam = (LPARAM)delItem;
			int iItem = FindItem(&find);
			if (iItem >= 0)
				DeleteItem(iItem);

			// finally it could be delete
			delete delItem;
		} else
			++it;
	}
}

bool CDownloadListCtrl::RemoveFile(const CPartFile *toremove)
{
	bool bResult = false;
	if (theApp.IsClosing())
		return bResult;
	// Retrieve all entries matching the File or linked to the file
	// Remark: The 'asked another files' clients must be removed from here
	ASSERT(toremove != NULL);
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end();) {
		const CtrlItem_Struct *delItem = it->second;
		if (delItem->owner == toremove || delItem->value == (void*)toremove) {
			// Remove it from the m_ListItems
			it = m_ListItems.erase(it);

			// Remove it from the CListCtrl
			LVFINDINFO find;
			find.flags = LVFI_PARAM;
			find.lParam = (LPARAM)delItem;
			int iItem = FindItem(&find);
			if (iItem >= 0)
				DeleteItem(iItem);

			// finally, it could be deleted
			delete delItem;
			bResult = true;
		} else
			++it;
	}
	ShowFilesCount();
	return bResult;
}

void CDownloadListCtrl::UpdateItem(void *toupdate)
{
	if (theApp.IsClosing())
		return;

	// Retrieve all entries matching the source
	for (ListItems::const_iterator it = m_ListItems.find(toupdate); it != m_ListItems.end() && it->first == toupdate; ++it) {
		CtrlItem_Struct *updateItem = it->second;

		// Find entry in CListCtrl and update object
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)updateItem;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			updateItem->dwUpdated = 0;
			Update(iItem);
		}
	}
	m_availableCommandsDirty = true;
}

void CDownloadListCtrl::DrawFileItem(CDC *dc, int nColumn, LPCRECT lpRect, UINT uDrawTextAlignment, CtrlItem_Struct *pCtrlItem)
{
	/*const*/ CPartFile *pPartFile = static_cast<CPartFile*>(pCtrlItem->value);
	const CString &sItem(GetFileItemDisplayText(pPartFile, nColumn));
	CRect rcDraw(lpRect);
	switch (nColumn) {
	case 0: // file name
		{
			int iIconPosY = (rcDraw.Height() > theApp.GetSmallSytemIconSize().cy) ? ((rcDraw.Height() - theApp.GetSmallSytemIconSize().cy) / 2) : 0;
			int iImage = theApp.GetFileTypeSystemImageIdx(pPartFile->GetFileName());
			if (theApp.GetSystemImageList() != NULL)
				::ImageList_Draw(theApp.GetSystemImageList(), iImage, dc->GetSafeHdc(), rcDraw.left, rcDraw.top + iIconPosY, ILD_TRANSPARENT);
			rcDraw.left += theApp.GetSmallSytemIconSize().cx;

			if (thePrefs.ShowRatingIndicator() && (pPartFile->HasComment() || pPartFile->HasRating() || pPartFile->IsKadCommentSearchRunning())) {
				m_ImageList.Draw(dc, pPartFile->UserRating(true) + 14, CPoint(rcDraw.left + 2, rcDraw.top + iIconPosY), ILD_NORMAL);
				rcDraw.left += 2 + RATING_ICON_WIDTH;
			}

			rcDraw.left += sm_iLabelOffset;
			dc->DrawText(sItem, -1, rcDraw, MLC_DT_TEXT | uDrawTextAlignment);
		}
		break;
	case 5: // progress
		{
			rcDraw.bottom--;
			rcDraw.top++;

			int iWidth = rcDraw.Width();
			int iHeight = rcDraw.Height();
			if (pCtrlItem->status == (HBITMAP)NULL)
				VERIFY(pCtrlItem->status.CreateBitmap(1, 1, 1, 8, NULL));
			CDC cdcStatus;
			HGDIOBJ hOldBitmap;
			cdcStatus.CreateCompatibleDC(dc);
			int cx = pCtrlItem->status.GetBitmapDimension().cx;
			DWORD dwTicks = ::GetTickCount();
			if (dwTicks >= pCtrlItem->dwUpdated + DLC_BARUPDATE || cx != iWidth || !pCtrlItem->dwUpdated) {
				pCtrlItem->status.DeleteObject();
				pCtrlItem->status.CreateCompatibleBitmap(dc, iWidth, iHeight);
				hOldBitmap = cdcStatus.SelectObject(pCtrlItem->status);

				CRect rec_status(0, 0, iWidth, iHeight);
				pPartFile->DrawStatusBar(&cdcStatus, rec_status, thePrefs.UseFlatBar());
				pCtrlItem->dwUpdated = dwTicks + (rand() & 0x7f);
			} else
				hOldBitmap = cdcStatus.SelectObject(pCtrlItem->status);
			dc->BitBlt(rcDraw.left, rcDraw.top, iWidth, iHeight, &cdcStatus, 0, 0, SRCCOPY);
			cdcStatus.SelectObject(hOldBitmap);

			if (thePrefs.GetUseDwlPercentage()) {
				COLORREF oldclr = dc->SetTextColor(RGB(255, 255, 255));
				int iOMode = dc->SetBkMode(TRANSPARENT);
				dc->DrawText(sItem.Mid(sItem.ReverseFind(_T(' ')) + 1), -1, rcDraw, (MLC_DT_TEXT & ~DT_LEFT) | DT_CENTER);
				dc->SetBkMode(iOMode);
				dc->SetTextColor(oldclr);
			}
		}
		break;
	default:
		dc->DrawText(sItem, -1, rcDraw, MLC_DT_TEXT | uDrawTextAlignment);
	}
}

CString CDownloadListCtrl::GetSourceItemDisplayText(const CtrlItem_Struct *pCtrlItem, int iSubItem)
{
	CString sText;
	const CUpDownClient *pClient = static_cast<CUpDownClient*>(pCtrlItem->value);
	switch (iSubItem) {
	case 0: //icon, name, status
		if (pClient->GetUserName())
			return CString(pClient->GetUserName());
		sText.Format(_T("(%s)"), (LPCTSTR)GetResString(IDS_UNKNOWN));
		break;
	case 1: //source from
		{
			UINT uid;
			switch (pClient->GetSourceFrom()) {
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
				uid = 0;
			}
			if (uid)
				return GetResString(uid);
		}
		break;
	case 2: //transferred
	case 3: //completed
		// - 'Transferred' column: Show transferred data
		// - 'Completed' column: If 'Transferred' column is hidden, show the amount of transferred data
		//	  in 'Completed' column. This is plain wrong (at least when receiving compressed data), but
		//	  users seem to got used to it.
		if (iSubItem == 2 || IsColumnHidden(2)) {
			if (pCtrlItem->type == AVAILABLE_SOURCE && pClient->GetTransferredDown())
				return CastItoXBytes(pClient->GetTransferredDown());
		}
		break;
	case 4: //speed
		if (pCtrlItem->type == AVAILABLE_SOURCE && pClient->GetDownloadDatarate())
			return CastItoXBytes(pClient->GetDownloadDatarate(), false, true);
		break;
	case 5: //file info
		return GetResString(IDS_DL_PROGRESS);
	case 6: //sources
		return pClient->GetClientSoftVer();
	case 7: //prio
		if (pClient->GetDownloadState() == DS_ONQUEUE) {
			if (pClient->IsRemoteQueueFull())
				return GetResString(IDS_QUEUEFULL);
			if (pClient->GetRemoteQueueRank())
				sText.Format(_T("QR: %u"), pClient->GetRemoteQueueRank());
		}
		break;
	case 8: //status
		{
			if (pCtrlItem->type == AVAILABLE_SOURCE)
				sText = pClient->GetDownloadStateDisplayString();
			else {
				sText = GetResString(IDS_ASKED4ANOTHERFILE);
// ZZ:DownloadManager -->
				if (thePrefs.IsExtControlsEnabled()) {
					UINT uid;
					if (pClient->IsInNoNeededList(pCtrlItem->owner))
						uid = IDS_NONEEDEDPARTS;
					else if (pClient->GetDownloadState() == DS_DOWNLOADING)
						uid = IDS_TRANSFERRING;
					else if (const_cast<CUpDownClient*>(pClient)->IsSwapSuspended(pClient->GetRequestFile()))
						uid = IDS_SOURCESWAPBLOCKED;
					else
						uid = 0;
					if (uid)
						sText.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(uid));
					if (pClient->GetRequestFile() && !pClient->GetRequestFile()->GetFileName().IsEmpty())
						sText.AppendFormat(_T(": \"%s\""), (LPCTSTR)pClient->GetRequestFile()->GetFileName());
				}
			}

			if (thePrefs.IsExtControlsEnabled() && !pClient->m_OtherRequests_list.IsEmpty())
				sText += _T('*');
// ZZ:DownloadManager <--
		}
	//	break;
	//case 9: //remaining time & size
	//case 10: //last seen complete
	//case 11: //last received
	//case 12: //category
	//case 13: //added on
	}
	return sText;
}

void CDownloadListCtrl::DrawSourceItem(CDC *dc, int nColumn, LPCRECT lpRect, UINT uDrawTextAlignment, CtrlItem_Struct *pCtrlItem)
{
	const CUpDownClient *pClient = static_cast<CUpDownClient*>(pCtrlItem->value);
	const CString &sItem(GetSourceItemDisplayText(pCtrlItem, nColumn));
	switch (nColumn) {
	case 0: // icon, name, status
		{
			CRect rcItem(*lpRect);
			int iIconPosY = (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 1;
			POINT point = {rcItem.left, rcItem.top + iIconPosY};
			int nImg;
			if (pCtrlItem->type == AVAILABLE_SOURCE) {
				switch (pClient->GetDownloadState()) {
				case DS_CONNECTED:
				case DS_CONNECTING:
				case DS_WAITCALLBACK:
				case DS_WAITCALLBACKKAD:
				case DS_TOOMANYCONNS:
				case DS_TOOMANYCONNSKAD:
					nImg = 2;
					break;
				case DS_ONQUEUE:
					nImg = pClient->IsRemoteQueueFull() ? 3 : 1;
					break;
				case DS_DOWNLOADING:
				case DS_REQHASHSET:
					nImg = 0;
					break;
				case DS_NONEEDEDPARTS:
				case DS_ERROR:
					nImg = 3;
					break;
				default:
					nImg = 4;
				}
			} else
				nImg = 3;
			m_ImageList.Draw(dc, nImg, point, ILD_NORMAL);
			rcItem.left += 20;

			UINT uOvlImg = 0;
			if ((pClient->Credits() && pClient->Credits()->GetCurrentIdentState(pClient->GetIP()) == IS_IDENTIFIED))
				uOvlImg |= 1;
			if (pClient->IsObfuscatedConnectionEstablished())
				uOvlImg |= 2;

			POINT point2 = {rcItem.left, rcItem.top + iIconPosY};
			if (pClient->IsFriend())
				nImg = 6;
			else
				switch (pClient->GetClientSoft()) {
				case SO_EDONKEYHYBRID:
					nImg = 9;
					break;
				case SO_MLDONKEY:
					nImg = 8;
					break;
				case SO_SHAREAZA:
					nImg = 10;
					break;
				case SO_URL:
					nImg = 11;
					break;
				case SO_AMULE:
					nImg = 12;
					break;
				case SO_LPHANT:
					nImg = 13;
					break;
				default:
					nImg = pClient->ExtProtocolAvailable() ? 5 : 7;
				}
			m_ImageList.Draw(dc, nImg, point2, ILD_NORMAL | INDEXTOOVERLAYMASK(uOvlImg));
			rcItem.left += 20;

			dc->DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
		}
		break;
	case 5: // file info
		{
			CRect rcDraw(lpRect);
			rcDraw.bottom--;
			rcDraw.top++;

			int iWidth = rcDraw.Width();
			int iHeight = rcDraw.Height();
			if (pCtrlItem->status == (HBITMAP)NULL)
				VERIFY(pCtrlItem->status.CreateBitmap(1, 1, 1, 8, NULL));
			CDC cdcStatus;
			HGDIOBJ hOldBitmap;
			cdcStatus.CreateCompatibleDC(dc);
			int cx = pCtrlItem->status.GetBitmapDimension().cx;
			DWORD dwTicks = ::GetTickCount();
			if (dwTicks >= pCtrlItem->dwUpdated + DLC_BARUPDATE || cx != iWidth || !pCtrlItem->dwUpdated) {
				pCtrlItem->status.DeleteObject();
				pCtrlItem->status.CreateCompatibleBitmap(dc, iWidth, iHeight);
				hOldBitmap = cdcStatus.SelectObject(pCtrlItem->status);

				CRect rec_status(0, 0, iWidth, iHeight);
				pClient->DrawStatusBar(&cdcStatus, rec_status, (pCtrlItem->type == UNAVAILABLE_SOURCE), thePrefs.UseFlatBar());
				pCtrlItem->dwUpdated = dwTicks + (rand() & 0x7f);
			} else
				hOldBitmap = cdcStatus.SelectObject(pCtrlItem->status);
			dc->BitBlt(rcDraw.left, rcDraw.top, iWidth, iHeight, &cdcStatus, 0, 0, SRCCOPY);
			cdcStatus.SelectObject(hOldBitmap);
		}
		break;
	//case 9: // remaining time & size
	//case 10: // last seen complete
	//case 11: // last received
	//case 12: // category
	//case 13: // added on
	//	break;
	default:
		dc->DrawText(sItem, -1, const_cast<LPRECT>(lpRect), MLC_DT_TEXT | uDrawTextAlignment);
	}
}

void CDownloadListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	RECT rcItem(lpDrawItemStruct->rcItem);
	CRect rcClient;
	GetClientRect(&rcClient);
	CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(lpDrawItemStruct->itemData);
	if (m_pFontBold)
		if (content->type == FILE_TYPE && static_cast<CPartFile*>(content->value)->GetTransferringSrcCount()
			|| ((content->type == UNAVAILABLE_SOURCE || content->type == AVAILABLE_SOURCE)
				&& static_cast<CUpDownClient*>(content->value)->GetDownloadState() == DS_DOWNLOADING))
		{
			dc.SelectObject(m_pFontBold);
		}
	BOOL notLast = lpDrawItemStruct->itemID + 1 != (UINT)GetItemCount();
	BOOL notFirst = lpDrawItemStruct->itemID != 0;
	int tree_start = 0;
	int tree_end = 0;

	int iTreeOffset = 6;
	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	rcItem.right = rcItem.left - sm_iLabelOffset;
	rcItem.left += sm_iIconOffset;

	if (content->type == FILE_TYPE) {
		if (!g_bLowColorDesktop && (lpDrawItemStruct->itemState & ODS_SELECTED) == 0) {
			DWORD dwCatColor = thePrefs.GetCatColor(static_cast<CPartFile*>(content->value)->GetCategory(), COLOR_WINDOWTEXT);
			if (dwCatColor > 0)
				dc.SetTextColor(dwCatColor);
		}

		for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
			int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
			if (!IsColumnHidden(iColumn)) {
				UINT uDrawTextAlignment;
				int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
				if (iColumn == 5) {
					int iNextLeft = rcItem.left + iColumnWidth;
					int iNextRight = rcItem.right + iColumnWidth;
					//set up tree vars
					rcItem.left = rcItem.right + iTreeOffset;
					rcItem.right = rcItem.left + min(8, iColumnWidth);
					tree_start = rcItem.left + 1;
					tree_end = rcItem.right;
					//normal column stuff
					rcItem.left = rcItem.right + 1;
					rcItem.right = tree_start + iColumnWidth - iTreeOffset;
					if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem))
						DrawFileItem(dc, 5, &rcItem, uDrawTextAlignment, content);
					rcItem.left = iNextLeft;
					rcItem.right = iNextRight;
				} else {
					rcItem.right += iColumnWidth;
					if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem))
						DrawFileItem(dc, iColumn, &rcItem, uDrawTextAlignment, content);
					if (iColumn == 0) {
						rcItem.left += sm_iLabelOffset;
						rcItem.right -= sm_iSubItemInset;
					}
					rcItem.left += iColumnWidth;
				}
			}
		}
	} else if (content->type == UNAVAILABLE_SOURCE || content->type == AVAILABLE_SOURCE) {
		for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
			int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
			if (!IsColumnHidden(iColumn)) {
				UINT uDrawTextAlignment;
				int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
				if (iColumn == 5) {
					int iNextLeft = rcItem.left + iColumnWidth;
					int iNextRight = rcItem.right + iColumnWidth;
					//set up tree vars
					rcItem.left = rcItem.right + iTreeOffset;
					rcItem.right = rcItem.left + min(8, iColumnWidth);
					tree_start = rcItem.left + 1;
					tree_end = rcItem.right;
					//normal column stuff
					rcItem.left = rcItem.right + 1;
					rcItem.right = tree_start + iColumnWidth - iTreeOffset;
					if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem))
						DrawSourceItem(dc, 5, &rcItem, uDrawTextAlignment, content);
					rcItem.left = iNextLeft;
					rcItem.right = iNextRight;
				} else {
					rcItem.right += iColumnWidth;
					if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem))
						DrawSourceItem(dc, iColumn, &rcItem, uDrawTextAlignment, content);
					if (iColumn == 0) {
						rcItem.left += sm_iLabelOffset;
						rcItem.right -= sm_iSubItemInset;
					}
					rcItem.left += iColumnWidth;
				}
			}
		}
	}

	DrawFocusRect(dc, &lpDrawItemStruct->rcItem, lpDrawItemStruct->itemState & ODS_FOCUS, bCtrlFocused, lpDrawItemStruct->itemState & ODS_SELECTED);

	//draw tree last so it draws over selected and focus (looks better)
	if (tree_start < tree_end) {
		//set new bounds
		RECT tree_rect = { tree_start, lpDrawItemStruct->rcItem.top, tree_end, lpDrawItemStruct->rcItem.bottom };
		dc.SetBoundsRect(&tree_rect, DCB_DISABLE);

		//gather some information
		BOOL hasNext = notLast &&
			reinterpret_cast<CtrlItem_Struct*>(GetItemData(lpDrawItemStruct->itemID + 1))->type != FILE_TYPE;
		BOOL isOpenRoot = hasNext && content->type == FILE_TYPE;
		BOOL isChild = content->type != FILE_TYPE;
		//BOOL isExpandable = !isChild && static_cast<CPartFile*>(content->value)->GetSourceCount() > 0;
		//might as well calculate these now
		int treeCenter = tree_start + 3;
		int middle = (rcItem.top + rcItem.bottom + 1) / 2;

		//set up a new pen for drawing the tree
		CPen pn, *oldpn;
		pn.CreatePen(PS_SOLID, 1, m_crWindowText);
		oldpn = dc.SelectObject(&pn);

		if (isChild) {
			//draw the line to the status bar
			dc.MoveTo(tree_end, middle);
			dc.LineTo(tree_start + 3, middle);

			//draw the line to the child node
			if (hasNext) {
				dc.MoveTo(treeCenter, middle);
				dc.LineTo(treeCenter, rcItem.bottom + 1);
			}
		} else if (isOpenRoot) {
			//draw circle
			RECT circle_rec = { treeCenter - 2, middle - 2, treeCenter + 3, middle + 3 };
			COLORREF crBk = dc.GetBkColor();
			dc.FrameRect(&circle_rec, &CBrush(m_crWindowText));
			dc.SetPixelV(circle_rec.left, circle_rec.top, crBk);
			dc.SetPixelV(circle_rec.right - 1, circle_rec.top, crBk);
			dc.SetPixelV(circle_rec.left, circle_rec.bottom - 1, crBk);
			dc.SetPixelV(circle_rec.right - 1, circle_rec.bottom - 1, crBk);
			//draw the line to the child node (hasNext is true here)
			dc.MoveTo(treeCenter, middle + 3);
			dc.LineTo(treeCenter, rcItem.bottom + 1);
		} /*else if(isExpandable) {
			//draw a + sign
			dc.MoveTo(treeCenter, middle - 2);
			dc.LineTo(treeCenter, middle + 3);
			dc.MoveTo(treeCenter - 2, middle);
			dc.LineTo(treeCenter + 3, middle);
		}*/

		//draw the line back up to parent node
		if (notFirst && isChild) {
			dc.MoveTo(treeCenter, middle);
			dc.LineTo(treeCenter, rcItem.top - 1);
		}

		//put the old pen back
		dc.SelectObject(oldpn);
		pn.DeleteObject();
	}
}

void CDownloadListCtrl::HideSources(CPartFile *toCollapse)
{
	SetRedraw(FALSE);
	for (int i = GetItemCount(); --i >= 0;) {
		CtrlItem_Struct *item = reinterpret_cast<CtrlItem_Struct*>(GetItemData(i));
		if (item != NULL && item->owner == toCollapse) {
			item->dwUpdated = 0;
			item->status.DeleteObject();
			DeleteItem(i);
		}
	}
	toCollapse->srcarevisible = false;
	SetRedraw(TRUE);
}

void CDownloadListCtrl::ExpandCollapseItem(int iItem, int iAction, bool bCollapseSource)
{
	if (iItem == -1)
		return;
	CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iItem));

	// to collapse/expand files when one of its source is selected
	if (content != NULL && bCollapseSource && content->parent != NULL) {
		content = content->parent;

		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)content;
		iItem = FindItem(&find);
		if (iItem == -1)
			return;
	}

	if (!content || content->type != FILE_TYPE)
		return;

	CPartFile *partfile = static_cast<CPartFile*>(content->value);
	if (!partfile)
		return;

	if (partfile->CanOpenFile()) {
		partfile->OpenFile();
		return;
	}

	// Check if the source branch is disable
	if (!partfile->srcarevisible) {
		if (iAction > COLLAPSE_ONLY) {
			SetRedraw(false);

			// Go through the whole list to find out the sources for this file
			// Remark: don't use GetSourceCount() => UNAVAILABLE_SOURCE
			for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
				const CtrlItem_Struct *cur_item = it->second;
				if (cur_item->owner == partfile) {
					partfile->srcarevisible = true;
					InsertItem(LVIF_PARAM | LVIF_TEXT, iItem + 1, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)cur_item);
				}
			}

			SetRedraw(true);
		}
	} else {
		if (iAction == EXPAND_COLLAPSE || iAction == COLLAPSE_ONLY) {
			if (GetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED) != (LVIS_SELECTED | LVIS_FOCUSED)) {
				SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				SetSelectionMark(iItem);
			}
			HideSources(partfile);
		}
	}
}

void CDownloadListCtrl::OnLvnItemActivate(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (thePrefs.IsDoubleClickEnabled() || pNMIA->iSubItem > 0)
		ExpandCollapseItem(pNMIA->iItem, EXPAND_COLLAPSE);
	*pResult = 0;
}

void CDownloadListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED);
	if (iSel >= 0) {
		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
		if (content != NULL && content->type == FILE_TYPE) {
			// get merged settings
			int iSelectedItems = 0;
			int iFilesNotDone = 0;
			int iFilesToPause = 0;
			int iFilesToStop = 0;
			int iFilesToResume = 0;
			int iFilesToOpen = 0;
			int iFilesGetPreviewParts = 0;
			int iFilesPreviewType = 0;
			int iFilesToPreview = 0;
			int iFilesToCancel = 0;
			int iFilesCanPauseOnPreview = 0;
			int iFilesDoPauseOnPreview = 0;
			int iFilesInCats = 0;
			int iFilesToImport = 0;
			UINT uPrioMenuItem = 0;
			const CPartFile *file1 = NULL;

			bool bFirstItem = true;
			for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
				const CtrlItem_Struct *pItemData = reinterpret_cast<CtrlItem_Struct*>(GetItemData(GetNextSelectedItem(pos)));
				if (pItemData == NULL || pItemData->type != FILE_TYPE)
					continue;
				const CPartFile *pFile = static_cast<CPartFile*>(pItemData->value);
				++iSelectedItems;

				iFilesToCancel += static_cast<int>(pFile->GetStatus() != PS_COMPLETING);
				iFilesNotDone += static_cast<int>((pFile->GetStatus() != PS_COMPLETE && pFile->GetStatus() != PS_COMPLETING));
				iFilesToStop += static_cast<int>(pFile->CanStopFile());
				iFilesToPause += static_cast<int>(pFile->CanPauseFile());
				iFilesToResume += static_cast<int>(pFile->CanResumeFile());
				iFilesToOpen += static_cast<int>(pFile->CanOpenFile());
				iFilesGetPreviewParts += static_cast<int>(pFile->GetPreviewPrio());
				iFilesPreviewType += static_cast<int>(pFile->IsPreviewableFileType());
				iFilesToPreview += static_cast<int>(pFile->IsReadyForPreview());
				iFilesCanPauseOnPreview += static_cast<int>(pFile->IsPreviewableFileType() && !pFile->IsReadyForPreview() && pFile->CanPauseFile());
				iFilesDoPauseOnPreview += static_cast<int>(pFile->IsPausingOnPreview());
				iFilesInCats += static_cast<int>(!pFile->HasDefaultCategory());
				iFilesToImport += static_cast<int>(pFile->GetFileOp() == PFOP_IMPORTPARTS);

				UINT uCurPrioMenuItem;
				if (pFile->IsAutoDownPriority())
					uCurPrioMenuItem = MP_PRIOAUTO;
				else if (pFile->GetDownPriority() == PR_HIGH)
					uCurPrioMenuItem = MP_PRIOHIGH;
				else if (pFile->GetDownPriority() == PR_NORMAL)
					uCurPrioMenuItem = MP_PRIONORMAL;
				else if (pFile->GetDownPriority() == PR_LOW)
					uCurPrioMenuItem = MP_PRIOLOW;
				else {
					uCurPrioMenuItem = 0;
					ASSERT(0);
				}

				if (bFirstItem) {
					bFirstItem = false;
					file1 = pFile;
					uPrioMenuItem = uCurPrioMenuItem;
				} else if (uPrioMenuItem != uCurPrioMenuItem)
					uPrioMenuItem = 0;
			}

			m_FileMenu.EnableMenuItem((UINT)m_PrioMenu.m_hMenu, iFilesNotDone > 0 ? MF_ENABLED : MF_GRAYED);
			m_PrioMenu.CheckMenuRadioItem(MP_PRIOLOW, MP_PRIOAUTO, uPrioMenuItem, 0);

			// enable commands if there is at least one item which can be used for the action
			m_FileMenu.EnableMenuItem(MP_CANCEL, iFilesToCancel > 0 ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_STOP, iFilesToStop > 0 ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_PAUSE, iFilesToPause > 0 ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_RESUME, iFilesToResume > 0 ? MF_ENABLED : MF_GRAYED);

			bool bOpenEnabled = (iSelectedItems == 1 && iFilesToOpen == 1);
			m_FileMenu.EnableMenuItem(MP_OPEN, bOpenEnabled ? MF_ENABLED : MF_GRAYED);

			CMenu PreviewWithMenu;
			PreviewWithMenu.CreateMenu();
			int iPreviewMenuEntries = thePreviewApps.GetAllMenuEntries(PreviewWithMenu, (iSelectedItems == 1) ? file1 : NULL);
			if (thePrefs.IsExtControlsEnabled()) {
				if (!thePrefs.GetPreviewPrio()) {
					m_PreviewMenu.EnableMenuItem(MP_TRY_TO_GET_PREVIEW_PARTS, (iSelectedItems == 1 && iFilesPreviewType == 1 && iFilesToPreview == 0 && iFilesNotDone == 1) ? MF_ENABLED : MF_GRAYED);
					m_PreviewMenu.CheckMenuItem(MP_TRY_TO_GET_PREVIEW_PARTS, (iSelectedItems == 1 && iFilesGetPreviewParts == 1) ? MF_CHECKED : MF_UNCHECKED);
				}
				m_PreviewMenu.EnableMenuItem(MP_PREVIEW, (iSelectedItems == 1 && iFilesToPreview == 1) ? MF_ENABLED : MF_GRAYED);
				m_PreviewMenu.EnableMenuItem(MP_PAUSEONPREVIEW, iFilesCanPauseOnPreview > 0 ? MF_ENABLED : MF_GRAYED);
				m_PreviewMenu.CheckMenuItem(MP_PAUSEONPREVIEW, (iSelectedItems > 0 && iFilesDoPauseOnPreview == iSelectedItems) ? MF_CHECKED : MF_UNCHECKED);
				m_FileMenu.EnableMenuItem((UINT)m_PreviewMenu.m_hMenu, m_PreviewMenu.HasEnabledItems() ? MF_ENABLED : MF_GRAYED);

				if (iPreviewMenuEntries > 0)
					if (!thePrefs.GetExtraPreviewWithMenu())
						m_PreviewMenu.InsertMenu(1, MF_POPUP | MF_BYPOSITION | (iSelectedItems == 1 ? MF_ENABLED : MF_GRAYED), (UINT_PTR)PreviewWithMenu.m_hMenu, GetResString(IDS_PREVIEWWITH));
					else
						m_FileMenu.InsertMenu(MP_METINFO, MF_POPUP | MF_BYCOMMAND | (iSelectedItems == 1 ? MF_ENABLED : MF_GRAYED), (UINT_PTR)PreviewWithMenu.m_hMenu, GetResString(IDS_PREVIEWWITH));
			} else {
				m_FileMenu.EnableMenuItem(MP_PREVIEW, (iSelectedItems == 1 && iFilesToPreview == 1) ? MF_ENABLED : MF_GRAYED);
				if (iPreviewMenuEntries > 0)
					m_FileMenu.InsertMenu(MP_METINFO, MF_POPUP | MF_BYCOMMAND | (iSelectedItems == 1 ? MF_ENABLED : MF_GRAYED), (UINT_PTR)PreviewWithMenu.m_hMenu, GetResString(IDS_PREVIEWWITH));
			}

			bool bDetailsEnabled = (iSelectedItems > 0);
			m_FileMenu.EnableMenuItem(MP_METINFO, bDetailsEnabled ? MF_ENABLED : MF_GRAYED);
			if (thePrefs.IsDoubleClickEnabled() && bOpenEnabled)
				m_FileMenu.SetDefaultItem(MP_OPEN);
			else if (!thePrefs.IsDoubleClickEnabled() && bDetailsEnabled)
				m_FileMenu.SetDefaultItem(MP_METINFO);
			else
				m_FileMenu.SetDefaultItem(UINT_MAX);
			m_FileMenu.EnableMenuItem(MP_VIEWFILECOMMENTS, (iSelectedItems >= 1 /*&& iFilesNotDone == 1*/) ? MF_ENABLED : MF_GRAYED);
			if (thePrefs.m_bImportParts) {
				m_FileMenu.RenameMenu(MP_IMPORTPARTS, MF_BYCOMMAND, GetResString(iFilesToImport > 0 ? IDS_IMPORTPARTS_STOP : IDS_IMPORTPARTS), _T("FILEIMPORTPARTS"));
				m_FileMenu.EnableMenuItem(MP_IMPORTPARTS, (thePrefs.m_bImportParts && iSelectedItems == 1 && iFilesNotDone == 1) ? MF_ENABLED : MF_GRAYED);
			}

			int total;
			m_FileMenu.EnableMenuItem(MP_CLEARCOMPLETED, GetCompleteDownloads(curTab, total) > 0 ? MF_ENABLED : MF_GRAYED);
			if (thePrefs.IsExtControlsEnabled()) {
				m_FileMenu.EnableMenuItem((UINT)m_SourcesMenu.m_hMenu, MF_ENABLED);
				m_SourcesMenu.EnableMenuItem(MP_ADDSOURCE, (iSelectedItems == 1 && iFilesToStop == 1) ? MF_ENABLED : MF_GRAYED);
				m_SourcesMenu.EnableMenuItem(MP_SETSOURCELIMIT, (iFilesNotDone == iSelectedItems) ? MF_ENABLED : MF_GRAYED);
			}

			m_FileMenu.EnableMenuItem(thePrefs.GetShowCopyEd2kLinkCmd() ? MP_GETED2KLINK : MP_SHOWED2KLINK, iSelectedItems > 0 ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_PASTE, theApp.IsEd2kFileLinkInClipboard() ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_FIND, GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED);
			m_FileMenu.EnableMenuItem(MP_SEARCHRELATED, theApp.emuledlg->searchwnd->CanSearchRelatedFiles() ? MF_ENABLED : MF_GRAYED);

			CTitleMenu WebMenu;
			WebMenu.CreateMenu();
			WebMenu.AddMenuTitle(NULL, true);
			int iWebMenuEntries = theWebServices.GetFileMenuEntries(&WebMenu);
			UINT flag = (iWebMenuEntries == 0 || iSelectedItems != 1) ? MF_GRAYED : MF_ENABLED;
			m_FileMenu.AppendMenu(MF_POPUP | flag, (UINT_PTR)WebMenu.m_hMenu, GetResString(IDS_WEBSERVICES), _T("WEB"));

			// create cat-submenu
			CMenu CatsMenu;
			CatsMenu.CreateMenu();
			FillCatsMenu(CatsMenu, iFilesInCats);
			m_FileMenu.AppendMenu(MF_POPUP, (UINT_PTR)CatsMenu.m_hMenu, GetResString(IDS_TOCAT), _T("CATEGORY"));

			bool bToolbarItem = !thePrefs.IsDownloadToolbarEnabled();
			if (bToolbarItem) {
				m_FileMenu.AppendMenu(MF_SEPARATOR);
				m_FileMenu.AppendMenu(MF_STRING, MP_TOGGLEDTOOLBAR, GetResString(IDS_SHOWTOOLBAR));
			}

			GetPopupMenuPos(*this, point);
			m_FileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
			if (bToolbarItem) {
				VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
				VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
			}
			VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
			VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
			if (iPreviewMenuEntries)
				if (thePrefs.IsExtControlsEnabled() && !thePrefs.GetExtraPreviewWithMenu())
					VERIFY(m_PreviewMenu.RemoveMenu((UINT)PreviewWithMenu.m_hMenu, MF_BYCOMMAND));
				else
					VERIFY(m_FileMenu.RemoveMenu((UINT)PreviewWithMenu.m_hMenu, MF_BYCOMMAND));

			VERIFY(WebMenu.DestroyMenu());
			VERIFY(CatsMenu.DestroyMenu());
			VERIFY(PreviewWithMenu.DestroyMenu());
		} else {
			const CUpDownClient *client = (content != NULL) ? static_cast<CUpDownClient*>(content->value) : NULL;
			const bool is_ed2k = client && client->IsEd2kClient();
			CTitleMenu ClientMenu;
			ClientMenu.CreatePopupMenu();
			ClientMenu.AddMenuTitle(GetResString(IDS_CLIENTS), true);
			ClientMenu.AppendMenu(MF_STRING, MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("CLIENTDETAILS"));
			ClientMenu.SetDefaultItem(MP_DETAIL);
			ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && !client->IsFriend()) ? MF_ENABLED : MF_GRAYED), MP_ADDFRIEND, GetResString(IDS_ADDFRIEND), _T("ADDFRIEND"));
			ClientMenu.AppendMenu(MF_STRING | (is_ed2k ? MF_ENABLED : MF_GRAYED), MP_MESSAGE, GetResString(IDS_SEND_MSG), _T("SENDMESSAGE"));
			ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetViewSharedFilesSupport()) ? MF_ENABLED : MF_GRAYED), MP_SHOWLIST, GetResString(IDS_VIEWFILES), _T("VIEWFILES"));
			if (Kademlia::CKademlia::IsRunning() && !Kademlia::CKademlia::IsConnected())
				ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a) ? MF_ENABLED : MF_GRAYED), MP_BOOT, GetResString(IDS_BOOTSTRAP));
			ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));

			CMenu A4AFMenu;
			A4AFMenu.CreateMenu();
			if (thePrefs.IsExtControlsEnabled()) {
// ZZ:DownloadManager -->
#ifdef _DEBUG
				if (content && content->type == UNAVAILABLE_SOURCE)
					A4AFMenu.AppendMenu(MF_STRING, MP_A4AF_CHECK_THIS_NOW, GetResString(IDS_A4AF_CHECK_THIS_NOW));
# endif
// <-- ZZ:DownloadManager
				if (A4AFMenu.GetMenuItemCount() > 0)
					ClientMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)A4AFMenu.m_hMenu, GetResString(IDS_A4AF));
			}

			GetPopupMenuPos(*this, point);
			ClientMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);

			VERIFY(A4AFMenu.DestroyMenu());
			VERIFY(ClientMenu.DestroyMenu());
		}
	} else { // nothing selected
		int total;
		m_FileMenu.EnableMenuItem((UINT)m_PrioMenu.m_hMenu, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_CANCEL, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_PAUSE, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_STOP, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_RESUME, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_OPEN, MF_GRAYED);

		if (thePrefs.IsExtControlsEnabled()) {
			m_FileMenu.EnableMenuItem((UINT)m_PreviewMenu.m_hMenu, MF_GRAYED);
			if (!thePrefs.GetPreviewPrio()) {
				m_PreviewMenu.EnableMenuItem(MP_TRY_TO_GET_PREVIEW_PARTS, MF_GRAYED);
				m_PreviewMenu.CheckMenuItem(MP_TRY_TO_GET_PREVIEW_PARTS, MF_UNCHECKED);
			}
			m_PreviewMenu.EnableMenuItem(MP_PREVIEW, MF_GRAYED);
			m_PreviewMenu.EnableMenuItem(MP_PAUSEONPREVIEW, MF_GRAYED);
		} else
			m_FileMenu.EnableMenuItem(MP_PREVIEW, MF_GRAYED);

		m_FileMenu.EnableMenuItem(MP_METINFO, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_VIEWFILECOMMENTS, MF_GRAYED);
		if (thePrefs.m_bImportParts)
			m_FileMenu.EnableMenuItem(MP_IMPORTPARTS, MF_GRAYED);

		m_FileMenu.EnableMenuItem(MP_CLEARCOMPLETED, GetCompleteDownloads(curTab, total) > 0 ? MF_ENABLED : MF_GRAYED);
		m_FileMenu.EnableMenuItem(thePrefs.GetShowCopyEd2kLinkCmd() ? MP_GETED2KLINK : MP_SHOWED2KLINK, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_PASTE, theApp.IsEd2kFileLinkInClipboard() ? MF_ENABLED : MF_GRAYED);
		m_FileMenu.SetDefaultItem(UINT_MAX);
		if (m_SourcesMenu)
			m_FileMenu.EnableMenuItem((UINT)m_SourcesMenu.m_hMenu, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_SEARCHRELATED, MF_GRAYED);
		m_FileMenu.EnableMenuItem(MP_FIND, GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED);

		// also show the "Web Services" entry, even if its disabled and therefore not usable, it though looks a little
		// less confusing this way.
		CTitleMenu WebMenu;
		WebMenu.CreateMenu();
		WebMenu.AddMenuTitle(NULL, true);
		theWebServices.GetFileMenuEntries(&WebMenu);
		m_FileMenu.AppendMenu(MF_POPUP | MF_GRAYED, (UINT_PTR)WebMenu.m_hMenu, GetResString(IDS_WEBSERVICES), _T("WEB"));

		bool bToolbarItem = !thePrefs.IsDownloadToolbarEnabled();
		if (bToolbarItem) {
			m_FileMenu.AppendMenu(MF_SEPARATOR);
			m_FileMenu.AppendMenu(MF_STRING, MP_TOGGLEDTOOLBAR, GetResString(IDS_SHOWTOOLBAR));
		}

		GetPopupMenuPos(*this, point);
		m_FileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
		if (bToolbarItem) {
			VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
			VERIFY(m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION));
		}
		m_FileMenu.RemoveMenu(m_FileMenu.GetMenuItemCount() - 1, MF_BYPOSITION);
		VERIFY(WebMenu.DestroyMenu());
	}
}

void CDownloadListCtrl::FillCatsMenu(CMenu &rCatsMenu, int iFilesInCats)
{
	ASSERT(rCatsMenu.m_hMenu);
	if (iFilesInCats == -1) {
		iFilesInCats = 0;
		int iSel = GetNextItem(-1, LVIS_SELECTED);
		if (iSel >= 0) {
			const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
			if (content != NULL && content->type == FILE_TYPE)
				for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
					const CtrlItem_Struct *pItemData = reinterpret_cast<CtrlItem_Struct*>(GetItemData(GetNextSelectedItem(pos)));
					if (pItemData != NULL && pItemData->type == FILE_TYPE) {
						const CPartFile *pFile = static_cast<CPartFile*>(pItemData->value);
						iFilesInCats += static_cast<int>((!pFile->HasDefaultCategory()));
					}
				}
		}
	}
	rCatsMenu.AppendMenu(MF_STRING, MP_NEWCAT, GetResString(IDS_NEW) + _T("..."));
	CString label = GetResString(IDS_CAT_UNASSIGN);
	label.Remove('(');
	label.Remove(')'); // Remove braces without having to put a new/changed resource string in
	rCatsMenu.AppendMenu(MF_STRING | ((iFilesInCats == 0) ? MF_GRAYED : MF_ENABLED), MP_ASSIGNCAT, label);
	if (thePrefs.GetCatCount() > 1) {
		rCatsMenu.AppendMenu(MF_SEPARATOR);
		for (int i = 1; i < thePrefs.GetCatCount(); ++i) {
			label = thePrefs.GetCategory(i)->strTitle;
			label.Replace(_T("&"), _T("&&"));
			rCatsMenu.AppendMenu(MF_STRING, MP_ASSIGNCAT + i, label);
		}
	}
}

CTitleMenu* CDownloadListCtrl::GetPrioMenu()
{
	UINT uPrioMenuItem = 0;
	int iSel = GetNextItem(-1, LVIS_SELECTED);
	if (iSel >= 0) {
		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
		if (content != NULL && content->type == FILE_TYPE) {
			bool bFirstItem = true;
			for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
				const CtrlItem_Struct *pItemData = reinterpret_cast<CtrlItem_Struct*>(GetItemData(GetNextSelectedItem(pos)));
				if (pItemData == NULL || pItemData->type != FILE_TYPE)
					continue;
				const CPartFile *pFile = static_cast<CPartFile*>(pItemData->value);
				UINT uCurPrioMenuItem;
				if (pFile->IsAutoDownPriority())
					uCurPrioMenuItem = MP_PRIOAUTO;
				else if (pFile->GetDownPriority() == PR_HIGH)
					uCurPrioMenuItem = MP_PRIOHIGH;
				else if (pFile->GetDownPriority() == PR_NORMAL)
					uCurPrioMenuItem = MP_PRIONORMAL;
				else if (pFile->GetDownPriority() == PR_LOW)
					uCurPrioMenuItem = MP_PRIOLOW;
				else {
					uCurPrioMenuItem = 0;
					ASSERT(0);
				}
				if (bFirstItem)
					uPrioMenuItem = uCurPrioMenuItem;
				else if (uPrioMenuItem != uCurPrioMenuItem) {
					uPrioMenuItem = 0;
					break;
				}
				bFirstItem = false;
			}
		}
	}
	m_PrioMenu.CheckMenuRadioItem(MP_PRIOLOW, MP_PRIOAUTO, uPrioMenuItem, 0);
	return &m_PrioMenu;
}

BOOL CDownloadListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	switch (wParam) {
	case MP_PASTE:
		if (theApp.IsEd2kFileLinkInClipboard())
			theApp.PasteClipboard(curTab);
		return TRUE;
	case MP_FIND:
		OnFindStart();
		return TRUE;
	case MP_TOGGLEDTOOLBAR:
		thePrefs.SetDownloadToolbar(true);
		theApp.emuledlg->transferwnd->ShowToolbar(true);
		return TRUE;
	}

	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel < 0)
		iSel = GetNextItem(-1, LVIS_SELECTED);
	if (iSel >= 0) {
		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
		if (content != NULL && content->type == FILE_TYPE) {
			//for multiple selections
			unsigned selectedCount = 0;
			CTypedPtrList<CPtrList, CPartFile*> selectedList;
			for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
				int index = GetNextSelectedItem(pos);
				if (index > -1 && reinterpret_cast<CtrlItem_Struct*>(GetItemData(index))->type == FILE_TYPE) {
					++selectedCount;
					selectedList.AddTail(static_cast<CPartFile*>(reinterpret_cast<CtrlItem_Struct*>(GetItemData(index))->value));
				}
			}

			CPartFile *file = static_cast<CPartFile*>(content->value);
			switch (wParam) {
			case MP_CANCEL:
			case MPG_DELETE: // keyboard del will continue to remove completed files from the screen while cancel will now also be available for complete files
				if (selectedCount > 0) {
					SetRedraw(false);
					CString fileList;
					bool validdelete = false;
					bool removecompl = false;
					int cFiles = 0;
					const int iMaxDisplayFiles = 10;
					for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL;) {
						const CPartFile *cur_file = selectedList.GetNext(pos);
						if (cur_file->GetStatus() != PS_COMPLETING && (cur_file->GetStatus() != PS_COMPLETE || wParam == MP_CANCEL)) {
							validdelete = true;
							if (++cFiles < iMaxDisplayFiles)
								fileList.AppendFormat(_T("\n%s"), (LPCTSTR)cur_file->GetFileName());
							else if (cFiles == iMaxDisplayFiles && pos != NULL)
								fileList += _T("\n...");
						} else if (cur_file->GetStatus() == PS_COMPLETE)
							removecompl = true;
					}
					CString quest(GetResString(selectedCount == 1 ? IDS_Q_CANCELDL2 : IDS_Q_CANCELDL));
					if ((removecompl && !validdelete) || (validdelete && AfxMessageBox(quest + fileList, MB_DEFBUTTON2 | MB_ICONQUESTION | MB_YESNO) == IDYES)) {
						bool bRemovedItems = !selectedList.IsEmpty();
						while (!selectedList.IsEmpty()) {
							CPartFile *partfile = selectedList.RemoveHead();
							HideSources(partfile);
							switch (partfile->GetStatus()) {
							case PS_WAITINGFORHASH:
							case PS_HASHING:
							case PS_COMPLETING:
								break;
							case PS_COMPLETE:
								if (wParam == MP_CANCEL) {
									bool delsucc = ShellDeleteFile(partfile->GetFilePath());
									if (delsucc)
										theApp.sharedfiles->RemoveFile(partfile, true);
									else {
										CString strError;
										strError.Format(GetResString(IDS_ERR_DELFILE) + _T("\r\n\r\n%s"), (LPCTSTR)partfile->GetFilePath(), (LPCTSTR)GetErrorMessage(::GetLastError()));
										AfxMessageBox(strError);
									}
								}
								RemoveFile(partfile);
								break;
							default:
								if (partfile->GetCategory())
									theApp.downloadqueue->StartNextFileIfPrefs(partfile->GetCategory());
							case PS_PAUSED:
								partfile->DeletePartFile();
							}
						}
						if (bRemovedItems) {
							AutoSelectItem();
							theApp.emuledlg->transferwnd->UpdateCatTabTitles();
						}
					}
					SetRedraw(true);
				}
				break;
			case MP_PRIOHIGH:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					partfile->SetAutoDownPriority(false);
					partfile->SetDownPriority(PR_HIGH);
				}
				SetRedraw(true);
				break;
			case MP_PRIOLOW:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					partfile->SetAutoDownPriority(false);
					partfile->SetDownPriority(PR_LOW);
				}
				SetRedraw(true);
				break;
			case MP_PRIONORMAL:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					partfile->SetAutoDownPriority(false);
					partfile->SetDownPriority(PR_NORMAL);
				}
				SetRedraw(true);
				break;
			case MP_PRIOAUTO:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					partfile->SetAutoDownPriority(true);
					partfile->SetDownPriority(PR_HIGH);
				}
				SetRedraw(true);
				break;
			case MP_PAUSE:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					if (partfile->CanPauseFile())
						partfile->PauseFile();
				}
				SetRedraw(true);
				break;
			case MP_RESUME:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					if (partfile->CanResumeFile())
						if (partfile->GetStatus() == PS_INSUFFICIENT)
							partfile->ResumeFileInsufficient();
						else
							partfile->ResumeFile();
				}
				SetRedraw(true);
				break;
			case MP_STOP:
				SetRedraw(false);
				while (!selectedList.IsEmpty()) {
					CPartFile *partfile = selectedList.RemoveHead();
					if (partfile->CanStopFile()) {
						HideSources(partfile);
						partfile->StopFile();
					}
				}
				SetRedraw(true);
				theApp.emuledlg->transferwnd->UpdateCatTabTitles();
				break;
			case MP_CLEARCOMPLETED:
				SetRedraw(false);
				ClearCompleted();
				SetRedraw(true);
				break;
			case MPG_F2:
				if (GetKeyState(VK_CONTROL) < 0 || selectedCount > 1) {
					// when ctrl is pressed -> filename cleanup
					if (IDYES == LocMessageBox(IDS_MANUAL_FILENAMECLEANUP, MB_YESNO, 0))
						while (!selectedList.IsEmpty()) {
							CPartFile *partfile = selectedList.RemoveHead();
							if (partfile->IsPartFile()) {
								HideSources(partfile);
								partfile->SetFileName(CleanupFilename(partfile->GetFileName()));
							}
						}
				} else {
					if (file->GetStatus() != PS_COMPLETE && file->GetStatus() != PS_COMPLETING) {
						InputBox inputbox;
						inputbox.SetLabels(GetResNoAmp(IDS_RENAME), GetResString(IDS_DL_FILENAME), file->GetFileName());
						inputbox.SetEditFilenameMode();
						if (inputbox.DoModal() == IDOK && !inputbox.GetInput().IsEmpty() && IsValidEd2kString(inputbox.GetInput())) {
							HideSources(file);
							file->SetFileName(inputbox.GetInput(), true);
							file->UpdateDisplayedInfo();
							file->SavePartFile();
						}
					} else
						MessageBeep(MB_OK);
				}
				break;
			case MP_METINFO:
			case MPG_ALTENTER:
				ShowFileDialog(0);
				break;
			case MP_COPYSELECTED:
			case MP_GETED2KLINK:
				{
					CString str;
					while (!selectedList.IsEmpty()) {
						const CAbstractFile *af = static_cast<CAbstractFile*>(selectedList.RemoveHead());
						if (af) {
							if (!str.IsEmpty())
								str += _T("\r\n");
							str += af->GetED2kLink();
						}
					}
					theApp.CopyTextToClipboard(str);
					break;
				}
			case MP_SEARCHRELATED:
				theApp.emuledlg->searchwnd->SearchRelatedFiles(selectedList);
				theApp.emuledlg->SetActiveDialog(theApp.emuledlg->searchwnd);
				break;
			case MP_OPEN:
			case IDA_ENTER:
				if (selectedCount == 1 && file->CanOpenFile())
					file->OpenFile();
				break;
			case MP_OPENFOLDER:
				if (selectedCount == 1)
					ShellOpenFile(file->GetPath());
				break;
			case MP_TRY_TO_GET_PREVIEW_PARTS:
				if (selectedCount == 1)
					file->SetPreviewPrio(!file->GetPreviewPrio());
				break;
			case MP_PREVIEW:
				if (selectedCount == 1)
					file->PreviewFile();
				break;
			case MP_PAUSEONPREVIEW:
				{
					bool bAllPausedOnPreview = true;
					for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL && bAllPausedOnPreview;)
						bAllPausedOnPreview = static_cast<CPartFile*>(selectedList.GetNext(pos))->IsPausingOnPreview();
					while (!selectedList.IsEmpty()) {
						CPartFile *pPartFile = selectedList.RemoveHead();
						if (pPartFile->IsPreviewableFileType() && !pPartFile->IsReadyForPreview())
							pPartFile->SetPauseOnPreview(!bAllPausedOnPreview);
					}
					break;
				}
			case MP_VIEWFILECOMMENTS:
				ShowFileDialog(IDD_COMMENTLST);
				break;
			case MP_IMPORTPARTS:
				if (!file->m_bMD4HashsetNeeded) //log "no hashset"?
					file->ImportParts();
				break;
			case MP_SHOWED2KLINK:
				ShowFileDialog(IDD_ED2KLINK);
				break;
			case MP_SETSOURCELIMIT:
				{
					CString temp;
					temp.Format(_T("%u"), file->GetPrivateMaxSources());
					InputBox inputbox;
					const CString &title(GetResString(IDS_SETPFSLIMIT));
					inputbox.SetLabels(title, GetResString(IDS_SETPFSLIMITEXPLAINED), temp);

					if (inputbox.DoModal() == IDOK) {
						int newlimit = _tstoi(inputbox.GetInput());
						while (!selectedList.IsEmpty()) {
							CPartFile *partfile = selectedList.RemoveHead();
							partfile->SetPrivateMaxSources(newlimit);
							partfile->UpdateDisplayedInfo(true);
						}
					}
					break;
				}
			case MP_ADDSOURCE:
				if (selectedCount == 1) {
					CAddSourceDlg as;
					as.SetFile(file);
					as.DoModal();
				}
				break;
			default:
				if (wParam >= MP_WEBURL && wParam <= MP_WEBURL + 99)
					theWebServices.RunURL(file, (UINT)wParam);
				else if ((wParam >= MP_ASSIGNCAT && wParam <= MP_ASSIGNCAT + 99) || wParam == MP_NEWCAT) {
					int nCatNumber;
					if (wParam == MP_NEWCAT) {
						nCatNumber = theApp.emuledlg->transferwnd->AddCategoryInteractive();
						if (nCatNumber == 0) // Creation canceled
							break;
					} else
						nCatNumber = (int)(wParam - MP_ASSIGNCAT);
					SetRedraw(FALSE);
					while (!selectedList.IsEmpty()) {
						CPartFile *partfile = selectedList.RemoveHead();
						partfile->SetCategory(nCatNumber);
						partfile->UpdateDisplayedInfo(true);
					}
					SetRedraw(TRUE);
					UpdateCurrentCategoryView();
					if (thePrefs.ShowCatTabInfos())
						theApp.emuledlg->transferwnd->UpdateCatTabTitles();
				} else if (wParam >= MP_PREVIEW_APP_MIN && wParam <= MP_PREVIEW_APP_MAX)
					thePreviewApps.RunApp(file, (UINT)wParam);
			}
		} else if (content != NULL) {
			CUpDownClient *client = static_cast<CUpDownClient*>(content->value);

			switch (wParam) {
			case MP_SHOWLIST:
				client->RequestSharedFileList();
				break;
			case MP_MESSAGE:
				theApp.emuledlg->chatwnd->StartSession(client);
				break;
			case MP_ADDFRIEND:
				if (theApp.friendlist->AddFriend(client))
					UpdateItem(client);
				break;
			case MP_DETAIL:
			case MPG_ALTENTER:
				ShowClientDialog(client);
				break;
			case MP_BOOT:
				if (client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a)
					Kademlia::CKademlia::Bootstrap(ntohl(client->GetIP()), client->GetKadPort());
// ZZ:DownloadManager -->
#ifdef _DEBUG
				break;
			case MP_A4AF_CHECK_THIS_NOW:
				{
					CPartFile *file = static_cast<CPartFile*>(content->owner);
					if (file->GetStatus(false) == PS_READY || file->GetStatus(false) == PS_EMPTY) {
						if (client->GetDownloadState() != DS_DOWNLOADING) {
							client->SwapToAnotherFile(_T("Manual init of source check. Test to be like ProcessA4AFClients(). CDownloadListCtrl::OnCommand() MP_SWAP_A4AF_DEBUG_THIS"), false, false, false, NULL, true, true, true); // ZZ:DownloadManager
							UpdateItem(file);
						}
					}
				}
#endif
// <-- ZZ:DownloadManager
			}
		}
	} else if (wParam == MP_CLEARCOMPLETED) //nothing selected
		ClearCompleted();

	m_availableCommandsDirty = true;
	return TRUE;
}

void CDownloadListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() == pNMListView->iSubItem)
		sortAscending = !GetSortAscending();
	else
		switch (pNMListView->iSubItem) {
		case 2: // Transferred
		case 3: // Completed
		case 4: // Download rate
		case 5: // Progress
		case 6: // Sources / Client Software
			sortAscending = false;
			break;
		case 9:
			// Keep the current 'm_bRemainSort' for that column, but reset to 'ascending'
		default:
			sortAscending = true;
		}

	// Ornis 4-way-sorting
	int adder = 0;
	if (pNMListView->iSubItem == 9) {
		if (GetSortItem() == 9 && sortAscending) // check for 'ascending' because the initial sort order is also 'ascending'
			m_bRemainSort = !m_bRemainSort;
		adder = m_bRemainSort ? 81 : 0;
	}

	// Sort table
	if (adder == 0)
		SetSortArrow(pNMListView->iSubItem, sortAscending);
	else
		SetSortArrow(pNMListView->iSubItem, sortAscending ? arrowDoubleUp : arrowDoubleDown);
	if (!sortAscending)
		adder += 100;
	UpdateSortHistory(pNMListView->iSubItem + adder);
	SortItems(SortProc, pNMListView->iSubItem + adder);

	// Save new preferences
	thePrefs.TransferlistRemainSortStyle(m_bRemainSort);

	*pResult = 0;
}

int CALLBACK CDownloadListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CtrlItem_Struct *item1 = reinterpret_cast<CtrlItem_Struct*>(lParam1);
	const CtrlItem_Struct *item2 = reinterpret_cast<CtrlItem_Struct*>(lParam2);

	int OrgSort = (int)lParamSort;
	int sortMod = 1;
	if (lParamSort >= 100) {
		sortMod = -1;
		lParamSort -= 100;
	}

	int comp;
	if (item1->type == FILE_TYPE && item2->type != FILE_TYPE) {
		if (item1->value == item2->parent->value)
			return -1;
		comp = Compare(static_cast<CPartFile*>(item1->value), static_cast<CPartFile*>(item2->parent->value), lParamSort);
	} else if (item2->type == FILE_TYPE && item1->type != FILE_TYPE) {
		if (item1->parent->value == item2->value)
			return 1;
		comp = Compare(static_cast<CPartFile*>(item1->parent->value), static_cast<CPartFile*>(item2->value), lParamSort);
	} else if (item1->type == FILE_TYPE) {
		const CPartFile *file1 = static_cast<CPartFile*>(item1->value);
		const CPartFile *file2 = static_cast<CPartFile*>(item2->value);
		comp = Compare(file1, file2, lParamSort);
	} else {
		if (item1->parent->value != item2->parent->value) {
			comp = Compare(static_cast<CPartFile*>(item1->parent->value), static_cast<CPartFile*>(item2->parent->value), lParamSort);
			return sortMod * comp;
		}
		if (item1->type != item2->type)
			return item1->type - item2->type;

		const CUpDownClient *client1 = static_cast<CUpDownClient*>(item1->value);
		const CUpDownClient *client2 = static_cast<CUpDownClient*>(item2->value);
		comp = Compare(client1, client2, lParamSort);
	}

	//call secondary sort order, if this one results in equal
	if (comp == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->GetDownloadList()->GetNextSortOrder(OrgSort);
		if (dwNextSort != -1)
			return SortProc(lParam1, lParam2, dwNextSort);
	}

	return sortMod * comp;
}

void CDownloadListCtrl::ClearCompleted(int incat)
{
	if (incat == -2)
		incat = curTab;

	// Search for completed file(s)
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end();) {
		const CtrlItem_Struct *cur_item = it->second;
		++it; // Already point to the next iterator.
		if (cur_item->type == FILE_TYPE) {
			CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (!file->IsPartFile() && (file->CheckShowItemInGivenCat(incat) || incat == -1))
				if (RemoveFile(file))
					it = m_ListItems.begin();
		}
	}
	if (thePrefs.ShowCatTabInfos())
		theApp.emuledlg->transferwnd->UpdateCatTabTitles();
}

void CDownloadListCtrl::ClearCompleted(const CPartFile *pFile)
{
	if (!pFile->IsPartFile())
		for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
			const CtrlItem_Struct *cur_item = it->second;
			if (cur_item->type == FILE_TYPE) {
				const CPartFile *pCurFile = static_cast<CPartFile*>(cur_item->value);
				if (pCurFile == pFile) {
					RemoveFile(pCurFile);
					return;
				}
			}
		}
}

void CDownloadListCtrl::SetStyle()
{
	if (thePrefs.IsDoubleClickEnabled())
		SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	else
		SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_ONECLICKACTIVATE);
}

void CDownloadListCtrl::OnListModified(LPNMHDR pNMHDR, LRESULT* /*pResult*/)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	//this works because true is equal to 1 and false equal to 0
	BOOL notLast = pNMListView->iItem + 1 != GetItemCount();
	BOOL notFirst = pNMListView->iItem != 0;
	RedrawItems(pNMListView->iItem - (int)notFirst, pNMListView->iItem + (int)notLast);
	m_availableCommandsDirty = true;
}

int CDownloadListCtrl::Compare(const CPartFile *file1, const CPartFile *file2, LPARAM lParamSort)
{
	switch (lParamSort) {
	case 0: //filename asc
		return CompareLocaleStringNoCase(file1->GetFileName(), file2->GetFileName());
	case 1: //size asc
		return CompareUnsigned64(file1->GetFileSize(), file2->GetFileSize());
	case 2: //transferred asc
		return CompareUnsigned64(file1->GetTransferred(), file2->GetTransferred());
	case 3: //completed asc
		return CompareUnsigned64(file1->GetCompletedSize(), file2->GetCompletedSize());
	case 4: //speed asc
		return CompareUnsigned(file1->GetDatarate(), file2->GetDatarate());
	case 5: //progress asc
		return sgn((float)file1->GetCompletedSize() / (float)file1->GetFileSize() - (float)file2->GetCompletedSize() / (float)file2->GetFileSize()); //compare exact ratio instead of rounded percents
	case 6: //sources asc
		return CompareUnsigned(file1->GetSourceCount(), file2->GetSourceCount());
	case 7: //priority asc
		return CompareUnsigned(file1->GetDownPriority(), file2->GetDownPriority());
	case 8: //Status asc
		return sgn(file1->getPartfileStatusRank() - file2->getPartfileStatusRank());
	case 9: //Remaining Time asc
		{
			//Make ascending sort so we can have the smaller remaining time on the top
			//instead of unknowns so we can see which files are about to finish better.
			time_t f1 = file1->getTimeRemaining();
			time_t f2 = file2->getTimeRemaining();
			//Same, do nothing.
			if (f1 == f2)
				break;

			//If descending, put first on top as it is unknown
			//If ascending, put first on bottom as it is unknown
			if (f1 == -1)
				return 1;

			//If descending, put second on top as it is unknown
			//If ascending, put second on bottom as it is unknown
			if (f2 == -1)
				return -1;

			//If descending, put first on top as it is bigger.
			//If ascending, put first on bottom as it is bigger.
			return CompareUnsigned((uint32)f1, (uint32)f2);
		}

	case 90: //Remaining SIZE asc
		return CompareUnsigned64(file1->GetFileSize() - file1->GetCompletedSize(), file2->GetFileSize() - file2->GetCompletedSize());
	case 10: //last seen complete asc
		return sgn(file1->lastseencomplete - file2->lastseencomplete);
	case 11: //last received Time asc
		return sgn(file1->GetLastReceptionDate() - file2->GetLastReceptionDate());
	case 12: //category
		//TODO: 'GetCategory' SHOULD be a 'const' function and 'GetResString' should NOT be called.
		return CompareLocaleStringNoCase((const_cast<CPartFile*>(file1)->GetCategory() != 0) ? thePrefs.GetCategory(const_cast<CPartFile*>(file1)->GetCategory())->strTitle : GetResString(IDS_ALL),
			(const_cast<CPartFile*>(file2)->GetCategory() != 0) ? thePrefs.GetCategory(const_cast<CPartFile*>(file2)->GetCategory())->strTitle : GetResString(IDS_ALL));
	case 13: // added on asc
		return sgn(file1->GetCrFileDate() - file2->GetCrFileDate());
	}
	return 0;
}

int CDownloadListCtrl::Compare(const CUpDownClient *client1, const CUpDownClient *client2, LPARAM lParamSort)
{
	switch (lParamSort) {
	case 0: //name asc
		if (client1->GetUserName() && client2->GetUserName())
			return CompareLocaleStringNoCase(client1->GetUserName(), client2->GetUserName());
		if (client1->GetUserName() == NULL)
			return 1; // place clients with no user names at bottom
		if (client2->GetUserName() == NULL)
			return -1; // place clients with no user names at bottom
		return 0;
	case 1: //size but we use status asc
		return client1->GetSourceFrom() - client2->GetSourceFrom();
	case 2: //transferred asc
	case 3: //completed asc
		return CompareUnsigned(client1->GetTransferredDown(), client2->GetTransferredDown());
	case 4: //speed asc
		return CompareUnsigned(client1->GetDownloadDatarate(), client2->GetDownloadDatarate());
	case 5: //progress asc
		return CompareUnsigned(client1->GetAvailablePartCount(), client2->GetAvailablePartCount());
	case 6:
		if (client1->GetClientSoft() == client2->GetClientSoft())
			return client1->GetVersion() - client2->GetVersion();
		return -(client1->GetClientSoft() - client2->GetClientSoft()); // invert result to place eMule's at top
	case 7: //qr asc
		if (client1->GetDownloadState() == DS_DOWNLOADING)
			return (client2->GetDownloadState() == DS_DOWNLOADING) ? 0 : -1;
		if (client2->GetDownloadState() == DS_DOWNLOADING)
			return 1;
		if (client1->GetRemoteQueueRank() == 0 && client1->GetDownloadState() == DS_ONQUEUE && client1->IsRemoteQueueFull())
			return 1;
		if (client2->GetRemoteQueueRank() == 0 && client2->GetDownloadState() == DS_ONQUEUE && client2->IsRemoteQueueFull())
			return -1;
		if (client1->GetRemoteQueueRank() == 0)
			return 1;
		if (client2->GetRemoteQueueRank() == 0)
			return -1;
		return CompareUnsigned(client1->GetRemoteQueueRank(), client2->GetRemoteQueueRank());
	case 8: //state asc
		if (client1->GetDownloadState() == client2->GetDownloadState()) {
			if (client1->IsRemoteQueueFull() && client2->IsRemoteQueueFull())
				return 0;
			if (client1->IsRemoteQueueFull())
				return 1;
			if (client2->IsRemoteQueueFull())
				return -1;
		}
		return client1->GetDownloadState() - client2->GetDownloadState();
	}
	return 0;
}

void CDownloadListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	int iSel = GetSelectionMark();
	if (iSel >= 0) {
		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
		if (content && content->value) {
			if (content->type == FILE_TYPE) {
				CPoint pt;
				if (!thePrefs.IsDoubleClickEnabled() && ::GetCursorPos(&pt)) {
					ScreenToClient(&pt);
					LVHITTESTINFO hit;
					hit.pt = pt;
					if (HitTest(&hit) >= 0 && (hit.flags & LVHT_ONITEM)) {
						LVHITTESTINFO subhit;
						subhit.pt = pt;
						if (SubItemHitTest(&subhit) >= 0 && subhit.iSubItem == 0) {
							CPartFile *file = static_cast<CPartFile*>(content->value);
							const LONG iconcx = theApp.GetSmallSytemIconSize().cx;
							if (thePrefs.ShowRatingIndicator()
								&& (file->HasComment() || file->HasRating() || file->IsKadCommentSearchRunning())
								&& pt.x >= sm_iIconOffset + iconcx
								&& pt.x <= sm_iIconOffset + iconcx + RATING_ICON_WIDTH)
							{
								ShowFileDialog(IDD_COMMENTLST);
							} else if (thePrefs.GetPreviewOnIconDblClk()
										&& pt.x >= sm_iIconOffset
										&& pt.x < sm_iIconOffset + iconcx)
							{
								if (file->IsReadyForPreview())
									file->PreviewFile();
								else
									MessageBeep(MB_OK);
							} else
								ShowFileDialog(0);
						}
					}
				}
			} else
				ShowClientDialog(static_cast<CUpDownClient*>(content->value));
		}
	}

	*pResult = 0;
}

void CDownloadListCtrl::CreateMenus()
{
	if (m_PreviewMenu)
		VERIFY(m_PreviewMenu.DestroyMenu());
	if (m_PrioMenu)
		VERIFY(m_PrioMenu.DestroyMenu());
	if (m_SourcesMenu)
		VERIFY(m_SourcesMenu.DestroyMenu());
	if (m_FileMenu)
		VERIFY(m_FileMenu.DestroyMenu());

	m_FileMenu.CreatePopupMenu();
	m_FileMenu.AddMenuTitle(GetResString(IDS_DOWNLOADMENUTITLE), true);

	// Add 'Download Priority' sub menu
	//
	m_PrioMenu.CreateMenu();
	m_PrioMenu.AddMenuTitle(NULL, true);
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOLOW, GetResString(IDS_PRIOLOW));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIONORMAL, GetResString(IDS_PRIONORMAL));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOHIGH, GetResString(IDS_PRIOHIGH));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOAUTO, GetResString(IDS_PRIOAUTO));

	CString sPrio(GetResString(IDS_PRIORITY));
	sPrio.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_DOWNLOAD));
	m_FileMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)m_PrioMenu.m_hMenu, sPrio, _T("FILEPRIORITY"));

	// Add file commands
	//
	m_FileMenu.AppendMenu(MF_STRING, MP_PAUSE, GetResString(IDS_DL_PAUSE), _T("PAUSE"));
	m_FileMenu.AppendMenu(MF_STRING, MP_STOP, GetResString(IDS_DL_STOP), _T("STOP"));
	m_FileMenu.AppendMenu(MF_STRING, MP_RESUME, GetResString(IDS_DL_RESUME), _T("RESUME"));
	m_FileMenu.AppendMenu(MF_STRING, MP_CANCEL, GetResString(IDS_MAIN_BTN_CANCEL), _T("DELETE"));
	m_FileMenu.AppendMenu(MF_SEPARATOR);

	m_FileMenu.AppendMenu(MF_STRING, MP_OPEN, GetResString(IDS_DL_OPEN), _T("OPENFILE"));
	// Extended: Submenu with Preview options, Normal: Preview and possibly 'Preview with' item
	if (thePrefs.IsExtControlsEnabled()) {
		m_PreviewMenu.CreateMenu();
		m_PreviewMenu.AddMenuTitle(NULL, true);
		m_PreviewMenu.AppendMenu(MF_STRING, MP_PREVIEW, GetResString(IDS_DL_PREVIEW), _T("PREVIEW"));
		m_PreviewMenu.AppendMenu(MF_STRING, MP_PAUSEONPREVIEW, GetResString(IDS_PAUSEONPREVIEW));
		if (!thePrefs.GetPreviewPrio())
			m_PreviewMenu.AppendMenu(MF_STRING, MP_TRY_TO_GET_PREVIEW_PARTS, GetResString(IDS_DL_TRY_TO_GET_PREVIEW_PARTS));
		m_FileMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)m_PreviewMenu.m_hMenu, GetResString(IDS_DL_PREVIEW), _T("PREVIEW"));
	} else
		m_FileMenu.AppendMenu(MF_STRING, MP_PREVIEW, GetResString(IDS_DL_PREVIEW), _T("PREVIEW"));

	m_FileMenu.AppendMenu(MF_STRING, MP_METINFO, GetResString(IDS_DL_INFO), _T("FILEINFO"));
	m_FileMenu.AppendMenu(MF_STRING, MP_VIEWFILECOMMENTS, GetResString(IDS_CMT_SHOWALL), _T("FILECOMMENTS"));
	if (thePrefs.m_bImportParts)
		m_FileMenu.AppendMenu(MF_STRING | MF_GRAYED, MP_IMPORTPARTS, GetResString(IDS_IMPORTPARTS), _T("FILEIMPORTPARTS"));
	m_FileMenu.AppendMenu(MF_SEPARATOR);

	m_FileMenu.AppendMenu(MF_STRING, MP_CLEARCOMPLETED, GetResString(IDS_DL_CLEAR), _T("CLEARCOMPLETE"));

	// Add (extended user mode) 'Source Handling' sub menu
	//
	if (thePrefs.IsExtControlsEnabled()) {
		m_SourcesMenu.CreateMenu();
		m_SourcesMenu.AppendMenu(MF_STRING, MP_ADDSOURCE, GetResString(IDS_ADDSRCMANUALLY));
		m_SourcesMenu.AppendMenu(MF_STRING, MP_SETSOURCELIMIT, GetResString(IDS_SETPFSLIMIT));
		m_FileMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)m_SourcesMenu.m_hMenu, GetResString(IDS_A4AF));
	}
	m_FileMenu.AppendMenu(MF_SEPARATOR);

	// Add 'Copy & Paste' commands
	//
	if (thePrefs.GetShowCopyEd2kLinkCmd())
		m_FileMenu.AppendMenu(MF_STRING, MP_GETED2KLINK, GetResString(IDS_DL_LINK1), _T("ED2KLINK"));
	else
		m_FileMenu.AppendMenu(MF_STRING, MP_SHOWED2KLINK, GetResString(IDS_DL_SHOWED2KLINK), _T("ED2KLINK"));
	m_FileMenu.AppendMenu(MF_STRING, MP_PASTE, GetResString(IDS_SW_DIRECTDOWNLOAD), _T("PASTELINK"));
	m_FileMenu.AppendMenu(MF_SEPARATOR);

	// Search commands
	//
	m_FileMenu.AppendMenu(MF_STRING, MP_FIND, GetResString(IDS_FIND), _T("Search"));
	m_FileMenu.AppendMenu(MF_STRING, MP_SEARCHRELATED, GetResString(IDS_SEARCHRELATED), _T("KadFileSearch"));
	// Web-services and categories will be added on-the-fly.
}

CString CDownloadListCtrl::getTextList()
{
	CString out;

	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			const CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			out.AppendFormat(_T("\n%s\t [%.1f%%] %u/%u - %s")
				, (LPCTSTR)file->GetFileName()
				, file->GetPercentCompleted()
				, file->GetTransferringSrcCount()
				, file->GetSourceCount()
				, (LPCTSTR)file->getPartfileStatus());
		}
	}
	return out;
}

float CDownloadListCtrl::GetFinishedSize()
{
	float fsize = 0;

	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			const CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (file->GetStatus() == PS_COMPLETE)
				fsize += (uint64)file->GetFileSize();
		}
	}
	return fsize;
}

int CDownloadListCtrl::GetFilesCountInCurCat()
{
	int iCount = 0;
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			iCount += static_cast<int>(file->CheckShowItemInGivenCat(curTab));
		}
	}
	return iCount;
}

CString CDownloadListCtrl::GetFileItemDisplayText(const CPartFile *lpPartFile, int iSubItem)
{
	CString sText;
	switch (iSubItem) {
	case 0: //file name
		sText = lpPartFile->GetFileName();
		break;
	case 1: //size
		sText = CastItoXBytes(lpPartFile->GetFileSize());
		break;
	case 2: //transferred
		sText = CastItoXBytes(lpPartFile->GetTransferred());
		break;
	case 3: //transferred complete
		sText = CastItoXBytes(lpPartFile->GetCompletedSize());
		break;
	case 4: //speed
		if (lpPartFile->GetTransferringSrcCount())
			sText = CastItoXBytes(lpPartFile->GetDatarate(), false, true);
		break;
	case 5: //progress
		sText.Format(_T("%s: %.1f%%"), (LPCTSTR)GetResString(IDS_DL_PROGRESS), lpPartFile->GetPercentCompleted());
		break;
	case 6: //sources
		{
// ZZ:DownloadManager -->
			const UINT sc = lpPartFile->GetSourceCount();
			if ((lpPartFile->GetStatus() != PS_PAUSED || sc) && lpPartFile->GetStatus() != PS_COMPLETE) {
				UINT ncsc = lpPartFile->GetNotCurrentSourcesCount();
				sText.Format(_T("%u"), sc - ncsc);
				if (ncsc > 0)
					sText.AppendFormat(_T("/%u"), sc);
				if (thePrefs.IsExtControlsEnabled() && lpPartFile->GetSrcA4AFCount() > 0)
					sText.AppendFormat(_T("+%u"), lpPartFile->GetSrcA4AFCount());
				if (lpPartFile->GetTransferringSrcCount() > 0)
					sText.AppendFormat(_T(" (%u)"), lpPartFile->GetTransferringSrcCount());
			}
// <-- ZZ:DownloadManager
			if (thePrefs.IsExtControlsEnabled() && lpPartFile->GetPrivateMaxSources() > 0)
				sText.AppendFormat(_T(" [%u]"), lpPartFile->GetPrivateMaxSources());
		}
		break;
	case 7: //prio
		{
			UINT uid;
			switch (lpPartFile->GetDownPriority()) {
			case PR_LOW:
				uid = lpPartFile->IsAutoDownPriority() ? IDS_PRIOAUTOLOW : IDS_PRIOLOW;
				break;
			case PR_NORMAL:
				uid = lpPartFile->IsAutoDownPriority() ? IDS_PRIOAUTONORMAL : IDS_PRIONORMAL;
				break;
			case PR_HIGH:
				uid = lpPartFile->IsAutoDownPriority() ? IDS_PRIOAUTOHIGH : IDS_PRIOHIGH;
				break;
			default:
				uid = 0;
			}
			if (uid)
				sText = GetResString(uid);
		}
		break;
	case 8: //state
		sText = lpPartFile->getPartfileStatus();
		break;
	case 9: //remaining time & size
		if (lpPartFile->GetStatus() != PS_COMPLETING && lpPartFile->GetStatus() != PS_COMPLETE) {
			time_t restTime = lpPartFile->getTimeRemaining();
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastSecondsToHM(restTime), (LPCTSTR)CastItoXBytes((uint64)(lpPartFile->GetFileSize() - lpPartFile->GetCompletedSize())));
		}
		break;
	case 10: //last seen complete
		if (lpPartFile->lastseencomplete == 0)
			sText = GetResString(IDS_NEVER);
		else
			sText = lpPartFile->lastseencomplete.Format(thePrefs.GetDateTimeFormat4Lists());
		if (lpPartFile->m_nCompleteSourcesCountLo == 0)
			sText.AppendFormat(_T(" (< %u)"), lpPartFile->m_nCompleteSourcesCountHi);
		else if (lpPartFile->m_nCompleteSourcesCountLo == lpPartFile->m_nCompleteSourcesCountHi)
			sText.AppendFormat(_T(" (%u)"), lpPartFile->m_nCompleteSourcesCountLo);
		else
			sText.AppendFormat(_T(" (%u - %u)"), lpPartFile->m_nCompleteSourcesCountLo, lpPartFile->m_nCompleteSourcesCountHi);
		break;
	case 11: //last receive
		if (lpPartFile->GetLastReceptionDate() == time_t(-1))
			sText = GetResString(IDS_NEVER);
		else
			sText = lpPartFile->GetCFileDate().Format(thePrefs.GetDateTimeFormat4Lists());
		break;
	case 12: //cat
		{
			UINT cat = const_cast<CPartFile*>(lpPartFile)->GetCategory();
			if (cat)
				sText = thePrefs.GetCategory(cat)->strTitle;
		}
		break;
	case 13: //added on
		if (lpPartFile->GetCrFileDate())
			sText = lpPartFile->GetCrCFileDate().Format(thePrefs.GetDateTimeFormat4Lists());
		else
			sText += _T('?');
	}
	return sText;
}

void CDownloadListCtrl::ShowFilesCount()
{
	theApp.emuledlg->transferwnd->UpdateFilesCount(GetFilesCountInCurCat());
}

void CDownloadListCtrl::ShowSelectedFileDetails()
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

	CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(GetSelectionMark()));
	if (content != NULL)
		if (content->type == FILE_TYPE) {
			const CPartFile *file = static_cast<CPartFile*>(content->value);
			bool b = (thePrefs.ShowRatingIndicator()
				&& (file->HasComment() || file->HasRating() || file->IsKadCommentSearchRunning())
				&& point.x >= sm_iIconOffset + theApp.GetSmallSytemIconSize().cx
				&& point.x <= sm_iIconOffset + theApp.GetSmallSytemIconSize().cx + RATING_ICON_WIDTH);
			ShowFileDialog(b ? IDD_COMMENTLST : 0);
		} else
			ShowClientDialog(static_cast<CUpDownClient*>(content->value));
}

int CDownloadListCtrl::GetCompleteDownloads(int cat, int &total)
{
	total = 0;
	int count = 0;
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			/*const*/ CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (file->CheckShowItemInGivenCat(cat) || cat == -1) {
				++total;
				count += static_cast<int>(file->GetStatus() == PS_COMPLETE);
			}
		}
	}
	return count;
}

void CDownloadListCtrl::UpdateCurrentCategoryView()
{
	ChangeCategory(curTab);
}

void CDownloadListCtrl::UpdateCurrentCategoryView(CPartFile *thisfile)
{
	ListItems::const_iterator it = m_ListItems.find(thisfile);
	if (it != m_ListItems.end()) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (!file->CheckShowItemInGivenCat(curTab))
				HideFile(file);
			else
				ShowFile(file);
		}
	}
}

void CDownloadListCtrl::ChangeCategory(int newsel)
{
	SetRedraw(FALSE);

	// remove all displayed files with a different cat and show the correct ones
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (!file->CheckShowItemInGivenCat(newsel))
				HideFile(file);
			else
				ShowFile(file);
		}
	}

	SetRedraw(TRUE);
	curTab = newsel;
	ShowFilesCount();
}

void CDownloadListCtrl::HideFile(CPartFile *tohide)
{
	HideSources(tohide);

	// Retrieve all entries matching the source
	for (ListItems::const_iterator it = m_ListItems.find(tohide); it != m_ListItems.end() && it->first == tohide; ++it) {
		CtrlItem_Struct *updateItem = it->second;

		// Find entry in CListCtrl and update object
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)updateItem;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			DeleteItem(iItem);
			return;
		}
	}
}

void CDownloadListCtrl::ShowFile(CPartFile *toshow)
{
	ListItems::const_iterator it = m_ListItems.find(toshow);
	if (it != m_ListItems.end()) {
		CtrlItem_Struct *updateItem = it->second;

		// Check if entry is already in the List
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)updateItem;
		if (FindItem(&find) == -1)
			InsertItem(LVIF_PARAM | LVIF_TEXT, GetItemCount(), LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)updateItem);
	}
}

void CDownloadListCtrl::GetDisplayedFiles(CArray<CPartFile*, CPartFile*> *list)
{
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE)
			list->Add(static_cast<CPartFile*>(cur_item->value));
	}
}

void CDownloadListCtrl::MoveCompletedfilesCat(UINT from, UINT to)
{
	const UINT cmin = min(from, to);
	const UINT cmax = max(from, to);
	for (ListItems::const_iterator it = m_ListItems.begin(); it != m_ListItems.end(); ++it) {
		const CtrlItem_Struct *cur_item = it->second;
		if (cur_item->type == FILE_TYPE) {
			CPartFile *file = static_cast<CPartFile*>(cur_item->value);
			if (!file->IsPartFile()) {
				UINT mycat = file->GetCategory();
				if (mycat >= cmin && mycat <= cmax)
					if (mycat == from)
						mycat = to;
					else
						mycat += (from < to ? -1 : 1);
				file->SetCategory(mycat);
			}
		}
	}
}

void CDownloadListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
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
		const LVITEM &rItem = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item;
		/*TRACE("CDownloadListCtrl::OnLvnGetDispInfo iItem=%d iSubItem=%d", rItem.iItem, rItem.iSubItem);
		if (rItem.mask & LVIF_TEXT)
			TRACE(" LVIF_TEXT");
		if (rItem.mask & LVIF_IMAGE)
			TRACE(" LVIF_IMAGE");
		if (rItem.mask & LVIF_STATE)
			TRACE(" LVIF_STATE");
		TRACE("\n");*/
		if (rItem.mask & LVIF_TEXT) {
			const CtrlItem_Struct *pCtrlItem = reinterpret_cast<CtrlItem_Struct*>(rItem.lParam);
			if (pCtrlItem != NULL && pCtrlItem->value != NULL)
				switch (pCtrlItem->type) {
				case FILE_TYPE:
					_tcsncpy_s(rItem.pszText, rItem.cchTextMax, GetFileItemDisplayText(static_cast<CPartFile*>(pCtrlItem->value), rItem.iSubItem), _TRUNCATE);
					break;
				case UNAVAILABLE_SOURCE:
				case AVAILABLE_SOURCE:
					_tcsncpy_s(rItem.pszText, rItem.cchTextMax, GetSourceItemDisplayText(pCtrlItem, rItem.iSubItem), _TRUNCATE);
					break;
				default:
					ASSERT(0);
				}
		}
	}
	*pResult = 0;
}

void CDownloadListCtrl::OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);
	LVHITTESTINFO hti;
	if (pGetInfoTip && pGetInfoTip->iSubItem == 0 && ::GetCursorPos(&hti.pt)) {
		ScreenToClient(&hti.pt);
		if (SubItemHitTest(&hti) == -1 || hti.iItem != pGetInfoTip->iItem || hti.iSubItem != 0) {
			// don't show the default label tip for the main item, if the mouse is not over the main item
			if ((pGetInfoTip->dwFlags & LVGIT_UNFOLDED) == 0 && pGetInfoTip->cchTextMax > 0 && pGetInfoTip->pszText[0] != _T('\0'))
				pGetInfoTip->pszText[0] = _T('\0');
			return;
		}

		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(pGetInfoTip->iItem));
		if (content && pGetInfoTip->pszText && pGetInfoTip->cchTextMax > 0) {
			CString info;

			// build info text and display it
			if (content->type == 1) { // for downloading files
				const CPartFile *partfile = static_cast<CPartFile*>(content->value);
				info = partfile->GetInfoSummary();
			} else if (content->type == 3 || content->type == 2) { // for sources
				const CUpDownClient *client = static_cast<CUpDownClient*>(content->value);
				if (client->IsEd2kClient()) {
					in_addr server;
					server.s_addr = client->GetServerIP();
					info.Format(GetResString(IDS_USERINFO) + _T("%s:%s:%u\n\n")
						, client->GetUserName() ? client->GetUserName() : (LPCTSTR)(_T('(') + GetResString(IDS_UNKNOWN) + _T(')'))
						, (LPCTSTR)GetResString(IDS_SERVER)
						, (LPCTSTR)ipstr(server)
						, client->GetServerPort());
					if (client->GetDownloadState() != DS_CONNECTING && client->GetDownloadState() != DS_DOWNLOADING) { //do not display inappropriate 'next re-ask'
						info.AppendFormat(GetResString(IDS_NEXT_REASK) + _T(":%s"), (LPCTSTR)CastSecondsToHM(client->GetTimeUntilReask(client->GetRequestFile()) / SEC2MS(1)));
						if (thePrefs.IsExtControlsEnabled())
							info.AppendFormat(_T(" (%s)"), (LPCTSTR)CastSecondsToHM(client->GetTimeUntilReask(content->owner) / SEC2MS(1)));
						info += _T('\n');
					}
					info.AppendFormat(GetResString(IDS_SOURCEINFO), client->GetAskedCountDown(), client->GetAvailablePartCount());
					info += _T('\n');

					if (content->type == 2) {
						info.AppendFormat(_T("%s%s"), (LPCTSTR)GetResString(IDS_CLIENTSOURCENAME), client->GetClientFilename().IsEmpty() ? _T("-") : (LPCTSTR)client->GetClientFilename());
						if (!client->GetFileComment().IsEmpty())
							info.AppendFormat(_T("\n%s %s"), (LPCTSTR)GetResString(IDS_CMT_READ), (LPCTSTR)client->GetFileComment());
						if (client->GetFileRating())
							info.AppendFormat(_T("\n%s:%s"), (LPCTSTR)GetResString(IDS_QL_RATING), (LPCTSTR)GetRateString(client->GetFileRating()));
					} else { // client asked twice
						info += GetResString(IDS_ASKEDFAF);
						if (client->GetRequestFile() && !client->GetRequestFile()->GetFileName().IsEmpty())
							info.AppendFormat(_T(": %s"), (LPCTSTR)client->GetRequestFile()->GetFileName());
					}

					if (thePrefs.IsExtControlsEnabled() && !client->m_OtherRequests_list.IsEmpty()) {
						CSimpleArray<const CString*> apstrFileNames;
						for (POSITION pos = client->m_OtherRequests_list.GetHeadPosition(); pos != NULL;)
							apstrFileNames.Add(&client->m_OtherRequests_list.GetNext(pos)->GetFileName());
						Sort(apstrFileNames);
						if (content->type == 2)
							info += _T('\n');
						info.AppendFormat(_T("\n%s:"), (LPCTSTR)GetResString(IDS_A4AF_FILES));

						for (int i = 0; i < apstrFileNames.GetSize(); ++i) {
							const CString *pstrFileName = apstrFileNames[i];
							if (info.GetLength() + (i > 0 ? 2 : 0) + pstrFileName->GetLength() >= pGetInfoTip->cchTextMax) {
								static TCHAR const szEllipsis[] = _T("\n:...");
								if (info.GetLength() + (int)_countof(szEllipsis) - 1 < pGetInfoTip->cchTextMax)
									info += szEllipsis;
								break;
							}
							if (i > 0)
								info += _T("\n:");
							info += *pstrFileName;
						}
					}
				} else
					info.Format(_T("URL: %s\nAvailable parts: %u"), client->GetUserName(), client->GetAvailablePartCount());
			}

			info += TOOLTIP_AUTOFORMAT_SUFFIX_CH;
			_tcsncpy(pGetInfoTip->pszText, info, pGetInfoTip->cchTextMax);
			pGetInfoTip->pszText[pGetInfoTip->cchTextMax - 1] = _T('\0');
		}
	}
	*pResult = 0;
}

void CDownloadListCtrl::ShowFileDialog(UINT uInvokePage)
{
	CSimpleArray<CPartFile*> aFiles;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
		int iItem = GetNextSelectedItem(pos);
		if (iItem >= 0) {
			const CtrlItem_Struct *pCtrlItem = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iItem));
			if (pCtrlItem != NULL && pCtrlItem->type == FILE_TYPE)
				aFiles.Add(static_cast<CPartFile*>(pCtrlItem->value));
		}
	}

	if (aFiles.GetSize() > 0) {
		CDownloadListListCtrlItemWalk::SetItemType(FILE_TYPE);
		CFileDetailDialog dialog(&aFiles, uInvokePage, this);
		dialog.DoModal();
	}
}

CDownloadListListCtrlItemWalk::CDownloadListListCtrlItemWalk(CDownloadListCtrl *pListCtrl)
	: CListCtrlItemWalk(pListCtrl)
	, m_pDownloadListCtrl(pListCtrl)
	, m_eItemType(INVALID_TYPE)
{
}

CObject* CDownloadListListCtrlItemWalk::GetPrevSelectableItem()
{
	if (m_pDownloadListCtrl == NULL) {
		ASSERT(0);
		return NULL;
	}
	ASSERT(m_eItemType != INVALID_TYPE);

	int iItemCount = m_pDownloadListCtrl->GetItemCount();
	if (iItemCount >= 2) {
		POSITION pos = m_pDownloadListCtrl->GetFirstSelectedItemPosition();
		if (pos) {
			int iItem = m_pDownloadListCtrl->GetNextSelectedItem(pos);
			int iCurSelItem = iItem;
			while (--iItem >= 0) {
				const CtrlItem_Struct *ctrl_item = reinterpret_cast<CtrlItem_Struct*>(m_pDownloadListCtrl->GetItemData(iItem));
				if (ctrl_item != NULL && (ctrl_item->type == m_eItemType || (m_eItemType != FILE_TYPE && ctrl_item->type != FILE_TYPE))) {
					m_pDownloadListCtrl->SetItemState(iCurSelItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
					m_pDownloadListCtrl->SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					m_pDownloadListCtrl->SetSelectionMark(iItem);
					m_pDownloadListCtrl->EnsureVisible(iItem, FALSE);
					return reinterpret_cast<CObject*>(ctrl_item->value);
				}
			}
		}
	}
	return NULL;
}

CObject* CDownloadListListCtrlItemWalk::GetNextSelectableItem()
{
	ASSERT(m_pDownloadListCtrl != NULL);
	if (m_pDownloadListCtrl == NULL)
		return NULL;
	ASSERT(m_eItemType != (ItemType)-1);

	int iItemCount = m_pDownloadListCtrl->GetItemCount();
	if (iItemCount >= 2) {
		POSITION pos = m_pDownloadListCtrl->GetFirstSelectedItemPosition();
		if (pos) {
			int iItem = m_pDownloadListCtrl->GetNextSelectedItem(pos);
			int iCurSelItem = iItem;
			while (++iItem < iItemCount) {
				const CtrlItem_Struct *ctrl_item = reinterpret_cast<CtrlItem_Struct*>(m_pDownloadListCtrl->GetItemData(iItem));
				if (ctrl_item != NULL && (ctrl_item->type == m_eItemType || (m_eItemType != FILE_TYPE && ctrl_item->type != FILE_TYPE))) {
					m_pDownloadListCtrl->SetItemState(iCurSelItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
					m_pDownloadListCtrl->SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					m_pDownloadListCtrl->SetSelectionMark(iItem);
					m_pDownloadListCtrl->EnsureVisible(iItem, FALSE);
					return reinterpret_cast<CObject*>(ctrl_item->value);
				}
			}
		}
	}
	return NULL;
}

void CDownloadListCtrl::ShowClientDialog(CUpDownClient *pClient)
{
	CDownloadListListCtrlItemWalk::SetItemType(AVAILABLE_SOURCE); // just set to something !=FILE_TYPE
	CClientDetailDialog dialog(pClient, this);
	dialog.DoModal();
}

CImageList* CDownloadListCtrl::CreateDragImage(int /*iItem*/, LPPOINT lpPoint)
{
	static const int iMaxSelectedItems = 30;
	int iSelectedItems = 0;
	CRect rcSelectedItems, rcLabel;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos && iSelectedItems < iMaxSelectedItems;) {
		int iItem = GetNextSelectedItem(pos);
		const CtrlItem_Struct *pCtrlItem = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iItem));
		if (pCtrlItem != NULL && pCtrlItem->type == FILE_TYPE && GetItemRect(iItem, rcLabel, LVIR_LABEL)) {
			if (iSelectedItems <= 0) {
				rcSelectedItems.left = sm_iIconOffset;
				rcSelectedItems.top = rcLabel.top;
				rcSelectedItems.right = rcLabel.right;
				rcSelectedItems.bottom = rcLabel.bottom;
			}
			rcSelectedItems.UnionRect(rcSelectedItems, rcLabel);
			++iSelectedItems;
		}
	}
	if (iSelectedItems <= 0)
		return NULL;

	CClientDC dc(this);
	CDC dcMem;
	if (!dcMem.CreateCompatibleDC(&dc))
		return NULL;

	CBitmap bmpMem;
	if (!bmpMem.CreateCompatibleBitmap(&dc, rcSelectedItems.Width(), rcSelectedItems.Height()))
		return NULL;

	CBitmap *pOldBmp = dcMem.SelectObject(&bmpMem);
	CFont *pOldFont = dcMem.SelectObject(GetFont());

	COLORREF crBackground = ::GetSysColor(COLOR_WINDOW);
	dcMem.FillSolidRect(0, 0, rcSelectedItems.Width(), rcSelectedItems.Height(), crBackground);
	dcMem.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));

	iSelectedItems = 0;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos && iSelectedItems < iMaxSelectedItems;) {
		int iItem = GetNextSelectedItem(pos);
		const CtrlItem_Struct *pCtrlItem = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iItem));
		if (pCtrlItem && pCtrlItem->type == FILE_TYPE) {
			const CPartFile *pPartFile = static_cast<CPartFile*>(pCtrlItem->value);
			GetItemRect(iItem, rcLabel, LVIR_LABEL);

			RECT rcItem;
			rcItem.left = 16 + sm_iLabelOffset;
			rcItem.top = rcLabel.top - rcSelectedItems.top;
			rcItem.right = rcLabel.right;
			rcItem.bottom = rcItem.top + rcLabel.Height();

			if (theApp.GetSystemImageList()) {
				int iImage = theApp.GetFileTypeSystemImageIdx(pPartFile->GetFileName());
				::ImageList_Draw(theApp.GetSystemImageList(), iImage, dcMem, 0, rcItem.top, ILD_TRANSPARENT);
			}

			dcMem.DrawText(pPartFile->GetFileName(), &rcItem, MLC_DT_TEXT);

			++iSelectedItems;
		}
	}
	dcMem.SelectObject(pOldBmp);
	dcMem.SelectObject(pOldFont);

	// At this point the bitmap in 'bmpMem' may or may not contain alpha data and we have to take special
	// care about passing such a bitmap further into Windows (GDI). Strange things can happen due to that
	// not all GDI functions can deal with RGBA bitmaps. Thus, create an image list with ILC_COLORDDB.
	CImageList *pimlDrag = new CImageList();
	pimlDrag->Create(rcSelectedItems.Width(), rcSelectedItems.Height(), ILC_COLORDDB | ILC_MASK, 1, 0);
	pimlDrag->Add(&bmpMem, crBackground);
	bmpMem.DeleteObject();

	if (lpPoint) {
		CPoint ptCursor;
		::GetCursorPos(&ptCursor);
		ScreenToClient(&ptCursor);
		lpPoint->x = ptCursor.x - rcSelectedItems.left;
		lpPoint->y = ptCursor.y - rcSelectedItems.top;
	}

	return pimlDrag;
}

bool CDownloadListCtrl::ReportAvailableCommands(CList<int> &liAvailableCommands)
{
	if (::GetTickCount() < m_dwLastAvailableCommandsCheck + SEC2MS(3) && !m_availableCommandsDirty)
		return false;
	m_dwLastAvailableCommandsCheck = ::GetTickCount();
	m_availableCommandsDirty = false;

	int iSel = GetNextItem(-1, LVIS_SELECTED);
	if (iSel >= 0) {
		const CtrlItem_Struct *content = reinterpret_cast<CtrlItem_Struct*>(GetItemData(iSel));
		if (content != NULL && content->type == FILE_TYPE) {
			// get merged settings
			int iSelectedItems = 0;
//			int iFilesNotDone = 0;
			int iFilesToPause = 0;
			int iFilesToStop = 0;
			int iFilesToResume = 0;
			int iFilesToOpen = 0;
//			int iFilesGetPreviewParts = 0;
//			int iFilesPreviewType = 0;
			int iFilesToPreview = 0;
			int iFilesToCancel = 0;
			for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
				const CtrlItem_Struct *pItemData = reinterpret_cast<CtrlItem_Struct*>(GetItemData(GetNextSelectedItem(pos)));
				if (pItemData == NULL || pItemData->type != FILE_TYPE)
					continue;
				const CPartFile *pFile = static_cast<CPartFile*>(pItemData->value);
				++iSelectedItems;

				iFilesToCancel += static_cast<int>(pFile->GetStatus() != PS_COMPLETING);
//				iFilesNotDone += static_cast<int>(pFile->GetStatus()!=PS_COMPLETE && pFile->GetStatus()!=PS_COMPLETING);
				iFilesToStop += static_cast<int>(pFile->CanStopFile());
				iFilesToPause += static_cast<int>(pFile->CanPauseFile());
				iFilesToResume += static_cast<int>(pFile->CanResumeFile());
				iFilesToOpen += static_cast<int>(pFile->CanOpenFile());
//				iFilesGetPreviewParts += static_cast<int>(pFile->GetPreviewPrio());
//				iFilesPreviewType += static_cast<int>(pFile->IsPreviewableFileType());
				iFilesToPreview += static_cast<int>(pFile->IsReadyForPreview());
			}


			// enable commands if there is at least one item which can be used for the action
			if (iFilesToCancel > 0)
				liAvailableCommands.AddTail(MP_CANCEL);
			if (iFilesToStop > 0)
				liAvailableCommands.AddTail(MP_STOP);
			if (iFilesToPause > 0)
				liAvailableCommands.AddTail(MP_PAUSE);
			if (iFilesToResume > 0)
				liAvailableCommands.AddTail(MP_RESUME);
			if (iSelectedItems == 1 && iFilesToOpen == 1)
				liAvailableCommands.AddTail(MP_OPEN);
			if (iSelectedItems == 1 && iFilesToPreview == 1)
				liAvailableCommands.AddTail(MP_PREVIEW);
			if (iSelectedItems == 1)
				liAvailableCommands.AddTail(MP_OPENFOLDER);
			if (iSelectedItems > 0) {
				liAvailableCommands.AddTail(MP_METINFO);
				liAvailableCommands.AddTail(MP_VIEWFILECOMMENTS);
				liAvailableCommands.AddTail(MP_SHOWED2KLINK);
				liAvailableCommands.AddTail(MP_NEWCAT);
				liAvailableCommands.AddTail(MP_PRIOLOW);
				if (theApp.emuledlg->searchwnd->CanSearchRelatedFiles())
					liAvailableCommands.AddTail(MP_SEARCHRELATED);
			}
		}
	}
	int total;
	if (GetCompleteDownloads(curTab, total) > 0)
		liAvailableCommands.AddTail(MP_CLEARCOMPLETED);
	if (GetItemCount() > 0)
		liAvailableCommands.AddTail(MP_FIND);
	return true;
}