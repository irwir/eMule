//this file is part of eMule
//Copyright (C)2002-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "SearchDlg.h"
#include "TransferWnd.h"
#include "TransferDlg.h"
#include "OtherFunctions.h"
#include "ClientList.h"
#include "UploadQueue.h"
#include "DownloadQueue.h"
#include "emuledlg.h"
#include "MenuCmds.h"
#include "PartFile.h"
#include "CatDialog.h"
#include "InputBox.h"
#include "UserMsgs.h"
#include "SharedFileList.h"
#include "DropDownButton.h"
#include "ToolTipCtrlX.h"
#include "SharedFilesWnd.h"
#include "HelpIDs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	DFLT_TOOLBAR_BTN_WIDTH	24

#define	WND_SPLITTER_YOFF	10
#define	WND_SPLITTER_HEIGHT	4

//#define	WND1_BUTTON_XOFF	8 use .rc file
#define	WND1_BUTTON_YOFF	5
#define	WND1_BUTTON_WIDTH	170
#define	WND1_BUTTON_HEIGHT	22	// don't set the height to something different than 22 unless you know exactly what you are doing!
#define	WND1_NUM_BUTTONS	6

//#define	WND2_BUTTON_XOFF	8 use .rc file
#define	WND2_BUTTON_WIDTH	170
#define	WND2_BUTTON_HEIGHT	22	// don't set the height to something different than 22 unless you know exactly what you are doing!
#define	WND2_NUM_BUTTONS	4

// CTransferWnd dialog

IMPLEMENT_DYNCREATE(CTransferWnd, CResizableFormView)

BEGIN_MESSAGE_MAP(CTransferWnd, CResizableFormView)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_DOWNLOADLIST, OnLvnBeginDragDownloadList)
	ON_NOTIFY(LVN_HOTTRACK, IDC_CLIENTLIST, OnHoverUploadList)
	ON_NOTIFY(LVN_HOTTRACK, IDC_DOWNLOADLIST, OnHoverDownloadList)
	ON_NOTIFY(LVN_HOTTRACK, IDC_QUEUELIST, OnHoverUploadList)
	ON_NOTIFY(LVN_HOTTRACK, IDC_UPLOADLIST, OnHoverUploadList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_DOWNLOADLIST, OnLvnKeydownDownloadList)
	ON_NOTIFY(NM_RCLICK, IDC_DLTAB, OnNmRClickDltab)
	ON_NOTIFY(TBN_DROPDOWN, IDC_DOWNLOAD_ICO, OnWnd1BtnDropDown)
	ON_NOTIFY(TBN_DROPDOWN, IDC_UPLOAD_ICO, OnWnd2BtnDropDown)
	ON_NOTIFY(TCN_SELCHANGE, IDC_DLTAB, OnTcnSelchangeDltab)
	ON_NOTIFY(UM_SPN_SIZED, IDC_SPLITTER, OnSplitterMoved)
	ON_NOTIFY(UM_TABMOVED, IDC_DLTAB, OnTabMovement)
	ON_WM_CTLCOLOR()
	ON_WM_HELPINFO()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETTINGCHANGE()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_PAINT()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

CTransferWnd::CTransferWnd(CWnd* /*pParent =NULL*/)
	: CResizableFormView(CTransferWnd::IDD)
	, m_uWnd2(wnd2Uploading)
	, downloadlistactive()
	, m_rightclickindex()
	, m_nDragIndex()
	, m_nDropIndex()
	, m_nLastCatTT(-1)
	, m_isetcatmenu()
	, m_bIsDragging()
	, m_pDragImage()
	, m_pLastMousePoint(POINT{-1, -1})
	, m_dwShowListIDC()
	, m_bLayoutInited()
{
	m_btnWnd1 = new CDropDownButton;
	m_btnWnd2 = new CDropDownButton;
	m_tooltipCats = new CToolTipCtrlX;
	SetImageList();
}

CTransferWnd::~CTransferWnd()
{
	delete m_btnWnd1;
	delete m_btnWnd2;
	delete m_tooltipCats;
	ASSERT(m_pDragImage == NULL);
	delete m_pDragImage;
}

void CTransferWnd::SetImageList()
{
	m_ImageList.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkey")));			//0 - eDonkey
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkeyPlus")));
	m_ImageList.Add(CTempIconLoader(_T("ClientCompatible")));		//2 - Compat
	m_ImageList.Add(CTempIconLoader(_T("ClientCompatiblePlus")));
	m_ImageList.Add(CTempIconLoader(_T("Friend")));					//4 - friend
	m_ImageList.Add(CTempIconLoader(_T("ClientMLDonkey")));			//5 - ML
	m_ImageList.Add(CTempIconLoader(_T("ClientMLDonkeyPlus")));
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkeyHybrid")));	//7 - Hybrid
	m_ImageList.Add(CTempIconLoader(_T("ClientEDonkeyHybridPlus")));
	m_ImageList.Add(CTempIconLoader(_T("ClientShareaza")));			//9 - Shareaza
	m_ImageList.Add(CTempIconLoader(_T("ClientShareazaPlus")));
	m_ImageList.Add(CTempIconLoader(_T("ClientAMule")));			//11 - amule
	m_ImageList.Add(CTempIconLoader(_T("ClientAMulePlus")));
	m_ImageList.Add(CTempIconLoader(_T("ClientLPhant")));			//13 - Lphant
	m_ImageList.Add(CTempIconLoader(_T("ClientLPhantPlus")));
	m_ImageList.Add(CTempIconLoader(_T("Server")));					//15 - http source
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("ClientSecureOvl"))), 1);
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("OverlayObfu"))), 2);
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("OverlaySecureObfu"))), 3);
}

void CTransferWnd::OnInitialUpdate()
{
	CResizableFormView::OnInitialUpdate();
	InitWindowStyles(this);

	ResetTransToolbar(thePrefs.IsTransToolbarEnabled(), false);
	uploadlistctrl.Init();
	downloadlistctrl.Init();
	queuelistctrl.Init();
	clientlistctrl.Init();
	downloadclientsctrl.Init();

	m_uWnd2 = (EWnd2)thePrefs.GetTransferWnd2();
	ShowWnd2(m_uWnd2);

	AddAnchor(IDC_DOWNLOADLIST, TOP_LEFT, ANCHOR(100, thePrefs.GetSplitterbarPosition()));
	AddAnchor(IDC_UPLOADLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_QUEUELIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_CLIENTLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_DOWNLOADCLIENTS, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_QUEUECOUNT, BOTTOM_LEFT);
	AddAnchor(IDC_QUEUECOUNT_LABEL, BOTTOM_LEFT);
	AddAnchor(IDC_QUEUE_REFRESH_BUTTON, BOTTOM_RIGHT);
	AddAnchor(IDC_DLTAB, TOP_RIGHT);

	static const uint32 uLists[6] = {
		  IDC_DOWNLOADLIST + IDC_UPLOADLIST	//0
		, IDC_DOWNLOADLIST					//1
		, IDC_UPLOADLIST					//2
		, IDC_QUEUELIST						//3
		, IDC_DOWNLOADCLIENTS				//4
		, IDC_CLIENTLIST};					//5
	UINT uid = thePrefs.GetTransferWnd1();
	m_dwShowListIDC = uLists[uid > 5 ? 0 : uid];

	//cats
	m_rightclickindex = -1;

	downloadlistactive = true;
	m_bIsDragging = false;

	// show & cat-tabs
	m_dlTab.ModifyStyle(0, TCS_OWNERDRAWFIXED);
	m_dlTab.SetPadding(CSize(6, 4));
	if (theApp.IsVistaThemeActive())
		m_dlTab.ModifyStyle(0, WS_CLIPCHILDREN);
	Category_Struct *cat0 = thePrefs.GetCategory(0);
	cat0->strTitle = GetCatTitle(cat0->filter);
	cat0->strIncomingPath = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR);
	cat0->care4all = true;

	for (int ix = 0; ix < thePrefs.GetCatCount(); ++ix)
		m_dlTab.InsertItem(ix, thePrefs.GetCategory(ix)->strTitle);

	// create tooltip control for download categories
	m_tooltipCats->Create(this, TTS_NOPREFIX);
	m_dlTab.SetToolTips(m_tooltipCats);
	UpdateCatTabTitles();
	UpdateTabToolTips();
	m_tooltipCats->SetMargin(CRect(4, 4, 4, 4));
	m_tooltipCats->SendMessage(TTM_SETMAXTIPWIDTH, 0, SHRT_MAX); // recognize \n chars!
	m_tooltipCats->SetDelayTime(TTDT_AUTOPOP, SEC2MS(20));
	m_tooltipCats->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	m_tooltipCats->Activate(TRUE);

	VerifyCatTabSize();
	Localize();
}

void CTransferWnd::ShowQueueCount(INT_PTR number)
{
	CString buffer;
	buffer.Format(_T("%u (%u %s)"), (unsigned)number, (unsigned)theApp.clientlist->GetBannedCount(), (LPCTSTR)GetResString(IDS_BANNED).MakeLower());
	SetDlgItemText(IDC_QUEUECOUNT, buffer);
}

void CTransferWnd::DoDataExchange(CDataExchange *pDX)
{
	CResizableFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DOWNLOAD_ICO, *m_btnWnd1);
	DDX_Control(pDX, IDC_UPLOAD_ICO, *m_btnWnd2);
	DDX_Control(pDX, IDC_DLTAB, m_dlTab);
	DDX_Control(pDX, IDC_UPLOADLIST, uploadlistctrl);
	DDX_Control(pDX, IDC_DOWNLOADLIST, downloadlistctrl);
	DDX_Control(pDX, IDC_QUEUELIST, queuelistctrl);
	DDX_Control(pDX, IDC_CLIENTLIST, clientlistctrl);
	DDX_Control(pDX, IDC_DOWNLOADCLIENTS, downloadclientsctrl);
}

void CTransferWnd::DoResize(int delta)
{
	CSplitterControl::ChangeHeight(&downloadlistctrl, delta);
	CSplitterControl::ChangeHeight(&uploadlistctrl, -delta, CW_BOTTOMALIGN);
	CSplitterControl::ChangeHeight(&queuelistctrl, -delta, CW_BOTTOMALIGN);
	CSplitterControl::ChangeHeight(&clientlistctrl, -delta, CW_BOTTOMALIGN);
	CSplitterControl::ChangeHeight(&downloadclientsctrl, -delta, CW_BOTTOMALIGN);
	CSplitterControl::ChangePos(m_btnWnd2, 0, delta);

	UpdateSplitterRange();

	if (m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST) {
		downloadlistctrl.Invalidate();
		downloadlistctrl.UpdateWindow();
		CHeaderCtrl *pHeaderUpdate = NULL;
		switch (m_uWnd2) {
		case wnd2Uploading:
			pHeaderUpdate = uploadlistctrl.GetHeaderCtrl();
			uploadlistctrl.Invalidate();
			uploadlistctrl.UpdateWindow();
			break;
		case wnd2OnQueue:
			pHeaderUpdate = queuelistctrl.GetHeaderCtrl();
			queuelistctrl.Invalidate();
			queuelistctrl.UpdateWindow();
			break;
		case wnd2Clients:
			pHeaderUpdate = clientlistctrl.GetHeaderCtrl();
			clientlistctrl.Invalidate();
			clientlistctrl.UpdateWindow();
			break;
		case wnd2Downloading:
			pHeaderUpdate = downloadclientsctrl.GetHeaderCtrl();
			downloadclientsctrl.Invalidate();
			downloadclientsctrl.UpdateWindow();
			break;
		default:
			ASSERT(0);
		}
		if (pHeaderUpdate != NULL) {
			pHeaderUpdate->Invalidate();
			pHeaderUpdate->UpdateWindow();
		}
	}
}

void CTransferWnd::UpdateSplitterRange()
{
	CRect rcWnd;
	GetWindowRect(rcWnd);
	if (rcWnd.Height() == 0) {
		ASSERT(0);
		return;
	}

	RECT rcDown;
	downloadlistctrl.GetWindowRect(&rcDown);
	ScreenToClient(&rcDown);

	RECT rcUp;
	downloadclientsctrl.GetWindowRect(&rcUp);
	ScreenToClient(&rcUp);

	thePrefs.SetSplitterbarPosition((rcDown.bottom * 100) / rcWnd.Height());

	RemoveAnchor(*m_btnWnd2);
	RemoveAnchor(IDC_DOWNLOADLIST);
	RemoveAnchor(IDC_UPLOADLIST);
	RemoveAnchor(IDC_QUEUELIST);
	RemoveAnchor(IDC_CLIENTLIST);
	RemoveAnchor(IDC_DOWNLOADCLIENTS);

	AddAnchor(*m_btnWnd2, ANCHOR(0, thePrefs.GetSplitterbarPosition()));
	AddAnchor(IDC_DOWNLOADLIST, TOP_LEFT, ANCHOR(100, thePrefs.GetSplitterbarPosition()));
	AddAnchor(IDC_UPLOADLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_QUEUELIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_CLIENTLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_DOWNLOADCLIENTS, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);

	m_wndSplitter.SetRange(rcDown.top + 50, rcUp.bottom - 40);
}

LRESULT CTransferWnd::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_WINDOWPOSCHANGED && m_wndSplitter)
		m_wndSplitter.Invalidate();

	return CResizableFormView::DefWindowProc(message, wParam, lParam);
}

void CTransferWnd::OnSplitterMoved(LPNMHDR pNMHDR, LRESULT* /*pResult*/)
{
	DoResize(reinterpret_cast<SPC_NMHDR*>(pNMHDR)->delta);
}

BOOL CTransferWnd::PreTranslateMessage(MSG *pMsg)
{
	if (theApp.emuledlg->m_pSplashWnd)
		return FALSE;
	if (pMsg->message == WM_KEYDOWN) {
		// Don't handle Ctrl+Tab in this window. It will be handled by main window.
		if (pMsg->wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0)
			return FALSE;
	} else if (pMsg->message == WM_LBUTTONDBLCLK) {
		if (pMsg->hwnd == m_dlTab.m_hWnd) {
			OnDblClickDltab();
			return TRUE;
		}
	} else if (pMsg->message == WM_MOUSEMOVE) {
		CPoint point;
		if (::GetCursorPos(&point) && (point.x != m_pLastMousePoint.x || point.y != m_pLastMousePoint.y)) {
			m_pLastMousePoint = point;
			// handle tooltip updating, when mouse is moved from one item to another
			m_nDropIndex = GetTabUnderMouse(point);
			if (m_nDropIndex != m_nLastCatTT) {
				m_nLastCatTT = m_nDropIndex;
				if (m_nDropIndex >= 0)
					UpdateTabToolTips(m_nDropIndex);
			}
		}
	} else if (!thePrefs.GetStraightWindowStyles() && pMsg->message == WM_MBUTTONUP) {
		if (downloadlistactive)
			downloadlistctrl.ShowSelectedFileDetails();
		else if (m_dwShowListIDC != IDC_DOWNLOADLIST + IDC_UPLOADLIST) {
			switch (m_dwShowListIDC) {
			case IDC_UPLOADLIST:
				uploadlistctrl.ShowSelectedUserDetails();
				break;
			case IDC_QUEUELIST:
				queuelistctrl.ShowSelectedUserDetails();
				break;
			case IDC_CLIENTLIST:
				clientlistctrl.ShowSelectedUserDetails();
				break;
			case IDC_DOWNLOADCLIENTS:
				downloadclientsctrl.ShowSelectedUserDetails();
				break;
			default:
				ASSERT(0);
			}
		} else {
			switch (m_uWnd2) {
			case wnd2OnQueue:
				queuelistctrl.ShowSelectedUserDetails();
				break;
			case wnd2Uploading:
				uploadlistctrl.ShowSelectedUserDetails();
				break;
			case wnd2Clients:
				clientlistctrl.ShowSelectedUserDetails();
				break;
			case wnd2Downloading:
				downloadclientsctrl.ShowSelectedUserDetails();
				break;
			default:
				ASSERT(0);
			}
		}
		return TRUE;
	}

	return CResizableFormView::PreTranslateMessage(pMsg);
}

int CTransferWnd::GetItemUnderMouse(CListCtrl &ctrl)
{
	CPoint pt;
	if (::GetCursorPos(&pt)) {
		ctrl.ScreenToClient(&pt);
		LVHITTESTINFO hit, subhit;
		hit.pt = pt;
		subhit.pt = pt;
		ctrl.SubItemHitTest(&subhit);
		int sel = ctrl.HitTest(&hit);
		if (sel != LB_ERR && (hit.flags & LVHT_ONITEM)) {
			if (subhit.iSubItem == 0)
				return sel;
		}
	}
	return LB_ERR;
}

void CTransferWnd::UpdateFilesCount(int iCount)
{
	if (m_dwShowListIDC == IDC_DOWNLOADLIST || m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST) {
		CString strBuffer;
		strBuffer.Format(_T("%s (%i)"), (LPCTSTR)GetResString(IDS_TW_DOWNLOADS), iCount);
		m_btnWnd1->SetWindowText(strBuffer);
	}
}

void CTransferWnd::UpdateListCount(EWnd2 listindex, int iCount /*=-1*/)
{
	switch (m_dwShowListIDC) {
	case IDC_DOWNLOADLIST + IDC_UPLOADLIST:
		{
			if (m_uWnd2 != listindex)
				return;
			CString strBuffer;
			switch (m_uWnd2) {
			case wnd2Uploading:
				{
					uint32 itemCount = (iCount == -1) ? uploadlistctrl.GetItemCount() : iCount;
					uint32 activeCount = (uint32)theApp.uploadqueue->GetActiveUploadsCount();
					if (activeCount >= itemCount)
						strBuffer.Format(_T(" (%u)"), itemCount);
					else
						strBuffer.Format(_T(" (%u/%u)"), activeCount, itemCount);
					m_btnWnd2->SetWindowText(GetResString(IDS_UPLOADING) + strBuffer);
				}
				break;
			case wnd2OnQueue:
				strBuffer.Format(_T(" (%i)"), iCount == -1 ? queuelistctrl.GetItemCount() : iCount);
				m_btnWnd2->SetWindowText(GetResString(IDS_ONQUEUE) + strBuffer);
				break;
			case wnd2Clients:
				strBuffer.Format(_T(" (%i)"), iCount == -1 ? clientlistctrl.GetItemCount() : iCount);
				m_btnWnd2->SetWindowText(GetResString(IDS_CLIENTLIST) + strBuffer);
				break;
			case wnd2Downloading:
				strBuffer.Format(_T(" (%i)"), iCount == -1 ? downloadclientsctrl.GetItemCount() : iCount);
				m_btnWnd2->SetWindowText(GetResString(IDS_DOWNLOADING) + strBuffer);
				break;
			default:
				ASSERT(0);
			}
		}
		break;
	case IDC_DOWNLOADLIST:
		break;
	case IDC_UPLOADLIST:
		if (listindex == wnd2Uploading) {
			CString strBuffer;
			uint32 itemCount = iCount == -1 ? uploadlistctrl.GetItemCount() : iCount;
			uint32 activeCount = (uint32)theApp.uploadqueue->GetActiveUploadsCount();
			if (activeCount >= itemCount)
				strBuffer.Format(_T(" (%u)"), itemCount);
			else
				strBuffer.Format(_T(" (%u/%u)"), activeCount, itemCount);
			m_btnWnd1->SetWindowText(GetResString(IDS_UPLOADING) + strBuffer);
		}
		break;
	case IDC_QUEUELIST:
		if (listindex == wnd2OnQueue) {
			CString strBuffer;
			strBuffer.Format(_T(" (%i)"), iCount == -1 ? queuelistctrl.GetItemCount() : iCount);
			m_btnWnd1->SetWindowText(GetResString(IDS_ONQUEUE) + strBuffer);
		}
		break;
	case IDC_CLIENTLIST:
		if (listindex == wnd2Clients) {
			CString strBuffer;
			strBuffer.Format(_T(" (%i)"), iCount == -1 ? clientlistctrl.GetItemCount() : iCount);
			m_btnWnd1->SetWindowText(GetResString(IDS_CLIENTLIST) + strBuffer);
		}
		break;
	case IDC_DOWNLOADCLIENTS:
		if (listindex == wnd2Downloading) {
			CString strBuffer;
			strBuffer.Format(_T(" (%i)"), iCount == -1 ? downloadclientsctrl.GetItemCount() : iCount);
			m_btnWnd1->SetWindowText(GetResString(IDS_DOWNLOADING) + strBuffer);
		}
	default:
		/*ASSERT(0)*/;
	}
}

void CTransferWnd::SwitchUploadList()
{
	if (m_uWnd2 == wnd2Uploading) {
		SetWnd2(wnd2Downloading);
		queuelistctrl.Hide();
		clientlistctrl.Hide();
		uploadlistctrl.Hide();
		downloadclientsctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_DOWNLOADING);
		SetWnd2Icon(w2iDownloading);
	} else if (m_uWnd2 == wnd2Downloading) {
		SetWnd2(wnd2OnQueue);
		if (thePrefs.IsQueueListDisabled()) {
			SwitchUploadList();
			return;
		}
		uploadlistctrl.Hide();
		clientlistctrl.Hide();
		downloadclientsctrl.Hide();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_SHOW);
		queuelistctrl.Show();
		m_btnWnd2->CheckButton(MP_VIEW2_ONQUEUE);
		SetWnd2Icon(w2iOnQueue);
	} else if (m_uWnd2 == wnd2OnQueue) {
		SetWnd2(wnd2Clients);
		if (thePrefs.IsKnownClientListDisabled()) {
			SwitchUploadList();
			return;
		}
		uploadlistctrl.Hide();
		queuelistctrl.Hide();
		downloadclientsctrl.Hide();
		clientlistctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_CLIENTS);
		SetWnd2Icon(w2iClientsKnown);
	} else {
		SetWnd2(wnd2Uploading);
		queuelistctrl.Hide();
		clientlistctrl.Hide();
		downloadclientsctrl.Hide();
		uploadlistctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_UPLOADING);
		SetWnd2Icon(w2iUploading);
	}
	UpdateListCount(m_uWnd2);
}

void CTransferWnd::ShowWnd2(EWnd2 uWnd2)
{
	if (uWnd2 == wnd2Downloading) {
		SetWnd2(uWnd2);
		queuelistctrl.Hide();
		clientlistctrl.Hide();
		uploadlistctrl.Hide();
		downloadclientsctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_DOWNLOADING);
		SetWnd2Icon(w2iDownloading);
	} else if (uWnd2 == wnd2OnQueue && !thePrefs.IsQueueListDisabled()) {
		SetWnd2(uWnd2);
		uploadlistctrl.Hide();
		clientlistctrl.Hide();
		downloadclientsctrl.Hide();
		queuelistctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_SHOW);
		m_btnWnd2->CheckButton(MP_VIEW2_ONQUEUE);
		SetWnd2Icon(w2iOnQueue);
	} else if (uWnd2 == wnd2Clients && !thePrefs.IsKnownClientListDisabled()) {
		SetWnd2(uWnd2);
		uploadlistctrl.Hide();
		queuelistctrl.Hide();
		downloadclientsctrl.Hide();
		clientlistctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_CLIENTS);
		SetWnd2Icon(w2iClientsKnown);
	} else {
		SetWnd2(wnd2Uploading);
		queuelistctrl.Hide();
		clientlistctrl.Hide();
		downloadclientsctrl.Hide();
		uploadlistctrl.Show();
		GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow(SW_HIDE);
		m_btnWnd2->CheckButton(MP_VIEW2_UPLOADING);
		SetWnd2Icon(w2iUploading);
	}
	UpdateListCount(m_uWnd2);
}

void CTransferWnd::SetWnd2(EWnd2 uWnd2)
{
	m_uWnd2 = uWnd2;
	thePrefs.SetTransferWnd2(m_uWnd2);
}

void CTransferWnd::OnSysColorChange()
{
	CResizableFormView::OnSysColorChange();
	SetAllIcons();
	m_btnWnd1->Invalidate();
	m_btnWnd2->Invalidate();
}

void CTransferWnd::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CResizableFormView::OnSettingChange(uFlags, lpszSection);
	// It does not work to reset the width of 1st button here.
	//m_btnWnd1->SetBtnWidth(IDC_DOWNLOAD_ICO, WND1_BUTTON_WIDTH);
	//m_btnWnd2->SetBtnWidth(IDC_UPLOAD_ICO, WND2_BUTTON_WIDTH);
}

void CTransferWnd::SetAllIcons()
{
	SetWnd1Icons();
	SetWnd2Icons();
}

void CTransferWnd::SetWnd1Icons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 1, 1);
	iml.Add(CTempIconLoader(_T("SplitWindow")));
	iml.Add(CTempIconLoader(_T("DownloadFiles")));
	iml.Add(CTempIconLoader(_T("Upload")));
	iml.Add(CTempIconLoader(_T("Download")));
	iml.Add(CTempIconLoader(_T("ClientsOnQueue")));
	iml.Add(CTempIconLoader(_T("ClientsKnown")));
	CImageList *pImlOld = m_btnWnd1->SetImageList(&iml);
	iml.Detach();
	if (pImlOld)
		pImlOld->DeleteImageList();
}

void CTransferWnd::SetWnd2Icons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 1, 1);
	iml.Add(CTempIconLoader(_T("Upload")));
	iml.Add(CTempIconLoader(_T("Download")));
	iml.Add(CTempIconLoader(_T("ClientsOnQueue")));
	iml.Add(CTempIconLoader(_T("ClientsKnown")));
	CImageList *pImlOld = m_btnWnd2->SetImageList(&iml);
	iml.Detach();
	if (pImlOld)
		pImlOld->DeleteImageList();
}

void CTransferWnd::Localize()
{
	LocalizeToolbars();
	SetDlgItemText(IDC_QUEUECOUNT_LABEL, GetResString(IDS_TW_QUEUE));
	SetDlgItemText(IDC_QUEUE_REFRESH_BUTTON, GetResString(IDS_SV_UPDATE));

	uploadlistctrl.Localize();
	queuelistctrl.Localize();
	downloadlistctrl.Localize();
	clientlistctrl.Localize();
	downloadclientsctrl.Localize();

	if (m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST)
		ShowSplitWindow();
	else
		ShowList(m_dwShowListIDC);
	UpdateListCount(m_uWnd2);
}

void CTransferWnd::LocalizeToolbars()
{
	m_btnWnd1->SetWindowText(GetResString(IDS_TW_DOWNLOADS));
	m_btnWnd1->SetBtnText(MP_VIEW1_SPLIT_WINDOW, GetResString(IDS_SPLIT_WINDOW));
	m_btnWnd1->SetBtnText(MP_VIEW1_DOWNLOADS, GetResString(IDS_TW_DOWNLOADS));
	m_btnWnd1->SetBtnText(MP_VIEW1_UPLOADING, GetResString(IDS_UPLOADING));
	m_btnWnd1->SetBtnText(MP_VIEW1_DOWNLOADING, GetResString(IDS_DOWNLOADING));
	m_btnWnd1->SetBtnText(MP_VIEW1_ONQUEUE, GetResString(IDS_ONQUEUE));
	m_btnWnd1->SetBtnText(MP_VIEW1_CLIENTS, GetResString(IDS_CLIENTLIST));
	m_btnWnd2->SetWindowText(GetResString(IDS_UPLOADING));
	m_btnWnd2->SetBtnText(MP_VIEW2_UPLOADING, GetResString(IDS_UPLOADING));
	m_btnWnd2->SetBtnText(MP_VIEW2_DOWNLOADING, GetResString(IDS_DOWNLOADING));
	m_btnWnd2->SetBtnText(MP_VIEW2_ONQUEUE, GetResString(IDS_ONQUEUE));
	m_btnWnd2->SetBtnText(MP_VIEW2_CLIENTS, GetResString(IDS_CLIENTLIST));
}

void CTransferWnd::OnBnClickedQueueRefreshButton()
{
	for (CUpDownClient *update = theApp.uploadqueue->GetNextClient(NULL); update != NULL; update = theApp.uploadqueue->GetNextClient(update))
		queuelistctrl.RefreshClient(update);
}

void CTransferWnd::OnHoverUploadList(LPNMHDR, LRESULT *pResult)
{
	downloadlistactive = false;
	*pResult = 0;
}

void CTransferWnd::OnHoverDownloadList(LPNMHDR, LRESULT *pResult)
{
	downloadlistactive = true;
	*pResult = 0;
}

void CTransferWnd::OnTcnSelchangeDltab(LPNMHDR, LRESULT *pResult)
{
	downloadlistctrl.ChangeCategory(m_dlTab.GetCurSel());
	*pResult = 0;
}

// Ornis' download categories
void CTransferWnd::OnNmRClickDltab(LPNMHDR, LRESULT *pResult)
{
	CPoint point;
	::GetCursorPos(&point);
	m_rightclickindex = GetTabUnderMouse(point);
	if (m_rightclickindex < 0)
		return;

	CMenu PrioMenu;
	PrioMenu.CreateMenu();
	Category_Struct *category_Struct = thePrefs.GetCategory(m_rightclickindex);
	PrioMenu.AppendMenu(MF_STRING, MP_PRIOLOW, GetResString(IDS_PRIOLOW));
	PrioMenu.CheckMenuItem(MP_PRIOLOW, category_Struct && category_Struct->prio == PR_LOW ? MF_CHECKED : MF_UNCHECKED);
	PrioMenu.AppendMenu(MF_STRING, MP_PRIONORMAL, GetResString(IDS_PRIONORMAL));
	PrioMenu.CheckMenuItem(MP_PRIONORMAL, category_Struct && category_Struct->prio != PR_LOW && category_Struct->prio != PR_HIGH ? MF_CHECKED : MF_UNCHECKED);
	PrioMenu.AppendMenu(MF_STRING, MP_PRIOHIGH, GetResString(IDS_PRIOHIGH));
	PrioMenu.CheckMenuItem(MP_PRIOHIGH, category_Struct && category_Struct->prio == PR_HIGH ? MF_CHECKED : MF_UNCHECKED);

	CTitleMenu menu;
	menu.CreatePopupMenu();
	CString sCat(GetResString(IDS_CAT));
	if (m_rightclickindex)
		sCat.AppendFormat(_T(" (%s)"), (LPCTSTR)thePrefs.GetCategory(m_rightclickindex)->strTitle);
	menu.AddMenuTitle(sCat, true);

	m_isetcatmenu = m_rightclickindex;
	CMenu CatMenu;
	CatMenu.CreateMenu();
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0, GetResString(IDS_ALL));
	UINT flag = (!thePrefs.GetCategory(m_rightclickindex)->care4all && m_rightclickindex) ? MF_GRAYED : MF_STRING;
	CatMenu.AppendMenu(flag, MP_CAT_SET0 + 1, GetResString(IDS_ALLOTHERS));

	// selector for regular expression view filter
	if (m_rightclickindex) {
		if (thePrefs.IsExtControlsEnabled())
			CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 18, GetResString(IDS_REGEXPRESSION));

		flag = (thePrefs.GetCategory(m_rightclickindex)->care4all) ? MF_STRING | MF_CHECKED | MF_BYCOMMAND : MF_STRING;

		if (thePrefs.IsExtControlsEnabled())
			CatMenu.AppendMenu(flag, MP_CAT_SET0 + 17, GetResString(IDS_CARE4ALL));
	}

	CatMenu.AppendMenu(MF_SEPARATOR);
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 2, GetResString(IDS_STATUS_NOTCOMPLETED));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 3, GetResString(IDS_DL_TRANSFCOMPL));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 4, GetResString(IDS_WAITING));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 5, GetResString(IDS_DOWNLOADING));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 6, GetResString(IDS_ERRORLIKE));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 7, GetResString(IDS_PAUSED));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 8, GetResString(IDS_SEENCOMPL));
	CatMenu.AppendMenu(MF_SEPARATOR);
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 10, GetResString(IDS_VIDEO));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 11, GetResString(IDS_AUDIO));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 12, GetResString(IDS_SEARCH_ARC));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 13, GetResString(IDS_SEARCH_CDIMG));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 14, GetResString(IDS_SEARCH_DOC));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 15, GetResString(IDS_SEARCH_PICS));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 16, GetResString(IDS_SEARCH_PRG));
	CatMenu.AppendMenu(MF_STRING, MP_CAT_SET0 + 20, GetResString(IDS_SEARCH_EMULECOLLECTION));

	if (thePrefs.IsExtControlsEnabled()) {
		CatMenu.AppendMenu(MF_SEPARATOR);
		CatMenu.AppendMenu(thePrefs.GetCatFilter(m_rightclickindex) > 0 ? MF_STRING : MF_GRAYED, MP_CAT_SET0 + 19, GetResString(IDS_NEGATEFILTER));
		if (thePrefs.GetCatFilterNeg(m_rightclickindex))
			CatMenu.CheckMenuItem(MP_CAT_SET0 + 19, MF_CHECKED | MF_BYCOMMAND);
	}

	CatMenu.CheckMenuItem(MP_CAT_SET0 + thePrefs.GetCatFilter(m_rightclickindex), MF_CHECKED | MF_BYCOMMAND);

	menu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)CatMenu.m_hMenu, GetResString(IDS_CHANGECATVIEW), _T("SEARCHPARAMS"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)PrioMenu.m_hMenu, GetResString(IDS_PRIORITY), _T("FILEPRIORITY"));
	menu.AppendMenu(MF_STRING, MP_CANCEL, GetResString(IDS_MAIN_BTN_CANCEL), _T("DELETE"));
	menu.AppendMenu(MF_STRING, MP_STOP, GetResString(IDS_DL_STOP), _T("STOP"));
	menu.AppendMenu(MF_STRING, MP_PAUSE, GetResString(IDS_DL_PAUSE), _T("PAUSE"));
	menu.AppendMenu(MF_STRING, MP_RESUME, GetResString(IDS_DL_RESUME), _T("RESUME"));
	menu.AppendMenu(MF_STRING, MP_RESUMENEXT, GetResString(IDS_DL_RESUMENEXT), _T("RESUME"));
	if (thePrefs.IsExtControlsEnabled() && m_rightclickindex != 0) {
		menu.AppendMenu(MF_STRING, MP_DOWNLOAD_ALPHABETICAL, GetResString(IDS_DOWNLOAD_ALPHABETICAL));
		menu.CheckMenuItem(MP_DOWNLOAD_ALPHABETICAL, category_Struct && category_Struct->downloadInAlphabeticalOrder ? MF_CHECKED : MF_UNCHECKED);
	}
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, MP_HM_OPENINC, GetResString(IDS_OPENINC), _T("Incoming"));

	flag = (m_rightclickindex == 0) ? MF_GRAYED : MF_STRING;
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, MP_CAT_ADD, GetResString(IDS_CAT_ADD));
	menu.AppendMenu(flag, MP_CAT_EDIT, GetResString(IDS_CAT_EDIT));
	menu.AppendMenu(flag, MP_CAT_REMOVE, GetResString(IDS_CAT_REMOVE));

	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);

	VERIFY(PrioMenu.DestroyMenu());
	VERIFY(CatMenu.DestroyMenu());
	VERIFY(menu.DestroyMenu());

	*pResult = 0;
}

void CTransferWnd::OnLvnBeginDragDownloadList(LPNMHDR pNMHDR, LRESULT *pResult)
{
	int iSel = downloadlistctrl.GetSelectionMark();
	if (iSel == -1)
		return;
	if (reinterpret_cast<CtrlItem_Struct*>(downloadlistctrl.GetItemData(iSel))->type != FILE_TYPE)
		return;

	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	m_nDragIndex = pNMLV->iItem;

	ASSERT(m_pDragImage == NULL);
	delete m_pDragImage;
	// It is actually more user friendly to attach the drag image (which could become
	// quite large) somewhat below the mouse cursor, instead of using the exact drag
	// position. When moving the drag image over the category tab control it is hard
	// to 'see' the tabs of the category control when the mouse cursor is in the middle
	// of the drag image.
	const bool bUseDragHotSpot = false;
	CPoint pt(0, -10); // default drag hot spot
	m_pDragImage = downloadlistctrl.CreateDragImage(m_nDragIndex, bUseDragHotSpot ? &pt : NULL);
	if (m_pDragImage == NULL) {
		// fall back code
		m_pDragImage = new CImageList();
		m_pDragImage->Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 0);
		m_pDragImage->Add(CTempIconLoader(_T("AllFiles")));
	}

	m_pDragImage->BeginDrag(0, pt);
	m_pDragImage->DragEnter(GetDesktopWindow(), pNMLV->ptAction);

	m_bIsDragging = true;
	m_nDropIndex = -1;
	SetCapture();

	*pResult = 0;
}

void CTransferWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!(nFlags & MK_LBUTTON))
		m_bIsDragging = false;

	if (m_bIsDragging) {
		ClientToScreen(&point);
		CPoint ptScreen(point);
		m_nDropIndex = GetTabUnderMouse(point);
		if (m_nDropIndex > 0 && thePrefs.GetCategory(m_nDropIndex)->care4all)	// not droppable
			m_dlTab.SetCurSel(-1);
		else
			m_dlTab.SetCurSel(m_nDropIndex);

		m_pDragImage->DragMove(ptScreen); //move the drag image to those coordinates
	}
}

void CTransferWnd::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/)
{
	if (m_bIsDragging) {
		ReleaseCapture();
		m_bIsDragging = false;
		m_pDragImage->DragLeave(GetDesktopWindow());
		m_pDragImage->EndDrag();
		delete m_pDragImage;
		m_pDragImage = NULL;

		if (m_nDropIndex >= 0 && (downloadlistctrl.curTab == 0 || (UINT)m_nDropIndex != downloadlistctrl.curTab)) {
			// for multiselections
			CTypedPtrList<CPtrList, CPartFile*> selectedList;
			for (POSITION pos = downloadlistctrl.GetFirstSelectedItemPosition(); pos != NULL;) {
				int index = downloadlistctrl.GetNextSelectedItem(pos);
				if (index >= 0) {
					const CtrlItem_Struct *pItem = reinterpret_cast<CtrlItem_Struct*>(downloadlistctrl.GetItemData(index));
					if (pItem->type == FILE_TYPE)
						selectedList.AddTail(reinterpret_cast<CPartFile*>(pItem->value));
				}
			}

			while (!selectedList.IsEmpty())
				selectedList.RemoveHead()->SetCategory(m_nDropIndex);
			m_dlTab.SetCurSel(downloadlistctrl.curTab);
			downloadlistctrl.UpdateCurrentCategoryView();
			UpdateCatTabTitles();
		} else
			m_dlTab.SetCurSel(downloadlistctrl.curTab);
		downloadlistctrl.Invalidate();
	}
}

BOOL CTransferWnd::OnCommand(WPARAM wParam, LPARAM)
{
	// category filter menuitems
	if (wParam >= MP_CAT_SET0 && wParam <= MP_CAT_SET0 + 99) {
		if (wParam == MP_CAT_SET0 + 17)
			thePrefs.GetCategory(m_isetcatmenu)->care4all = !thePrefs.GetCategory(m_isetcatmenu)->care4all;
		else if (wParam == MP_CAT_SET0 + 19) // negate
			thePrefs.SetCatFilterNeg(m_isetcatmenu, (!thePrefs.GetCatFilterNeg(m_isetcatmenu)));
		else { // set the view filter
			if (wParam - MP_CAT_SET0 < 1)	// don't negate all filter
				thePrefs.SetCatFilterNeg(m_isetcatmenu, false);
			thePrefs.SetCatFilter(m_isetcatmenu, (int)(wParam - MP_CAT_SET0));
			m_nLastCatTT = -1;
		}

		// set to regexp but none is set for that category?
		if (wParam == MP_CAT_SET0 + 18 && thePrefs.GetCategory(m_isetcatmenu)->regexp.IsEmpty()) {
			m_nLastCatTT = -1;
			CCatDialog dialog(m_rightclickindex);
			dialog.DoModal();

			// still no regexp?
			if (thePrefs.GetCategory(m_isetcatmenu)->regexp.IsEmpty())
				thePrefs.SetCatFilter(m_isetcatmenu, 0);
		}

		downloadlistctrl.UpdateCurrentCategoryView();
		EditCatTabLabel(m_isetcatmenu);
		thePrefs.SaveCats();
		return TRUE;
	}

	switch (wParam) {
	case MP_CAT_ADD:
		m_nLastCatTT = -1;
		AddCategoryInteractive();
		break;
	case MP_CAT_EDIT:
		{
			m_nLastCatTT = -1;
			const CString &oldincpath = thePrefs.GetCatPath(m_rightclickindex);
			CCatDialog dialog(m_rightclickindex);
			if (dialog.DoModal() == IDOK) {
				EditCatTabLabel(m_rightclickindex, thePrefs.GetCategory(m_rightclickindex)->strTitle);
				m_dlTab.SetTabTextColor(m_rightclickindex, thePrefs.GetCatColor(m_rightclickindex));
				theApp.emuledlg->searchwnd->UpdateCatTabs();
				downloadlistctrl.UpdateCurrentCategoryView();
				thePrefs.SaveCats();
				if (CompareDirectory(oldincpath, thePrefs.GetCatPath(m_rightclickindex)))
					theApp.emuledlg->sharedfileswnd->Reload();
			}
		}
		break;
	case MP_CAT_REMOVE:
		{
			m_nLastCatTT = -1;
			bool toreload = (_tcsicmp(thePrefs.GetCatPath(m_rightclickindex), thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) != 0);
			theApp.downloadqueue->ResetCatParts(m_rightclickindex);
			thePrefs.RemoveCat(m_rightclickindex);
			m_dlTab.DeleteItem(m_rightclickindex);
			m_dlTab.SetCurSel(0);
			downloadlistctrl.ChangeCategory(0);
			thePrefs.SaveCats();
			if (thePrefs.GetCatCount() == 1)
				thePrefs.GetCategory(0)->filter = 0;
			theApp.emuledlg->searchwnd->UpdateCatTabs();
			VerifyCatTabSize();
			if (toreload)
				theApp.emuledlg->sharedfileswnd->Reload();
		}
		break;

	case MP_PRIOLOW:
		thePrefs.GetCategory(m_rightclickindex)->prio = PR_LOW;
		thePrefs.SaveCats();
		break;
	case MP_PRIONORMAL:
		thePrefs.GetCategory(m_rightclickindex)->prio = PR_NORMAL;
		thePrefs.SaveCats();
		break;
	case MP_PRIOHIGH:
		thePrefs.GetCategory(m_rightclickindex)->prio = PR_HIGH;
		thePrefs.SaveCats();
		break;

	case MP_PAUSE:
		theApp.downloadqueue->SetCatStatus(m_rightclickindex, MP_PAUSE);
		break;
	case MP_STOP:
		theApp.downloadqueue->SetCatStatus(m_rightclickindex, MP_STOP);
		break;
	case MP_CANCEL:
		if (LocMessageBox(IDS_Q_CANCELDL, MB_ICONQUESTION | MB_YESNO, 0) == IDYES)
			theApp.downloadqueue->SetCatStatus(m_rightclickindex, MP_CANCEL);
		break;
	case MP_RESUME:
		theApp.downloadqueue->SetCatStatus(m_rightclickindex, MP_RESUME);
		break;
	case MP_RESUMENEXT:
		theApp.downloadqueue->StartNextFile(m_rightclickindex, false);
		break;

	case MP_DOWNLOAD_ALPHABETICAL:
		{
			bool newSetting = !thePrefs.GetCategory(m_rightclickindex)->downloadInAlphabeticalOrder;
			thePrefs.GetCategory(m_rightclickindex)->downloadInAlphabeticalOrder = newSetting;
			thePrefs.SaveCats();
			if (newSetting)	// any auto prio files will be set to normal now.
				theApp.downloadqueue->RemoveAutoPrioInCat(m_rightclickindex, PR_NORMAL);
		}
		break;

	case IDC_UPLOAD_ICO:
		SwitchUploadList();
		break;
	case MP_VIEW2_UPLOADING:
		ShowWnd2(wnd2Uploading);
		break;
	case MP_VIEW2_DOWNLOADING:
		ShowWnd2(wnd2Downloading);
		break;
	case MP_VIEW2_ONQUEUE:
		ShowWnd2(wnd2OnQueue);
		break;
	case MP_VIEW2_CLIENTS:
		ShowWnd2(wnd2Clients);
		break;
	case IDC_QUEUE_REFRESH_BUTTON:
		OnBnClickedQueueRefreshButton();
		break;

	case IDC_DOWNLOAD_ICO:
		OnBnClickedChangeView();
		break;
	case MP_VIEW1_SPLIT_WINDOW:
		ShowSplitWindow();
		break;
	case MP_VIEW1_DOWNLOADS:
		ShowList(IDC_DOWNLOADLIST);
		break;
	case MP_VIEW1_UPLOADING:
		ShowList(IDC_UPLOADLIST);
		break;
	case MP_VIEW1_DOWNLOADING:
		ShowList(IDC_DOWNLOADCLIENTS);
		break;
	case MP_VIEW1_ONQUEUE:
		ShowList(IDC_QUEUELIST);
		break;
	case MP_VIEW1_CLIENTS:
		ShowList(IDC_CLIENTLIST);
		break;

	case MP_HM_OPENINC:
		ShellOpenFile(thePrefs.GetCategory(m_isetcatmenu)->strIncomingPath);
	}
	return TRUE;
}

void CTransferWnd::UpdateCatTabTitles(bool force)
{
	CPoint point;
	::GetCursorPos(&point);
	if (!force && GetTabUnderMouse(point) >= 0)	// avoid cat tooltip jumping
		return;

	for (int i = m_dlTab.GetItemCount(); --i >= 0;) {
		EditCatTabLabel(i, /*(i==0) ? GetCatTitle(thePrefs.GetCategory(0)->filter) :*/thePrefs.GetCategory(i)->strTitle);
		m_dlTab.SetTabTextColor(i, thePrefs.GetCatColor(i));
	}
}

void CTransferWnd::EditCatTabLabel(int index)
{
	EditCatTabLabel(index, /*(index==0) ? GetCatTitle(thePrefs.GetAllcatType()) :*/thePrefs.GetCategory(index)->strTitle);
}

void CTransferWnd::EditCatTabLabel(int index, CString newlabel)
{
	TCITEM tabitem;
	tabitem.mask = TCIF_PARAM;
	m_dlTab.GetItem(index, &tabitem);
	tabitem.mask = TCIF_TEXT;

	newlabel.Replace(_T("&"), _T("&&"));

	if (!index)
		newlabel.Empty();

	if (!index || thePrefs.GetCatFilter(index) > 0) {

		if (index)
			newlabel += _T(" (");

		if (thePrefs.GetCatFilterNeg(index))
			newlabel += _T('!');

		if (thePrefs.GetCatFilter(index) == 18)
			newlabel.AppendFormat(_T("\"%s\""), (LPCTSTR)thePrefs.GetCategory(index)->regexp);
		else
			newlabel += GetCatTitle(thePrefs.GetCatFilter(index));

		if (index)
			newlabel += _T(')');
	}

	if (thePrefs.ShowCatTabInfos()) {
		int dwl = 0;
		for (POSITION pos = NULL; ;) {
			CPartFile *pFile = theApp.downloadqueue->GetFileNext(pos);
			dwl += static_cast<int>(pFile && pFile->CheckShowItemInGivenCat(index) && pFile->GetTransferringSrcCount() > 0);
			if (pos == NULL)
				break;
		}
		CString title(newlabel);
		int count;
		downloadlistctrl.GetCompleteDownloads(index, count);
		newlabel.Format(_T("%s %i/%i"), (LPCTSTR)title, dwl, count);
	}

	tabitem.pszText = newlabel.LockBuffer();
	m_dlTab.SetItem(index, &tabitem);
	newlabel.UnlockBuffer();

	VerifyCatTabSize();
}

int CTransferWnd::AddCategory(const CString &newtitle, const CString &newincoming, const CString &newcomment, const CString &newautocat, bool addTab)
{
	Category_Struct *newcat = new Category_Struct;
	newcat->strTitle = newtitle;
	newcat->prio = PR_NORMAL;
	newcat->strIncomingPath = newincoming;
	newcat->strComment = newcomment;
	newcat->regexp.Empty();
	newcat->ac_regexpeval = false;
	newcat->autocat = newautocat;
	newcat->downloadInAlphabeticalOrder = FALSE;
	newcat->filter = 0;
	newcat->filterNeg = false;
	newcat->care4all = false;
	newcat->color = CLR_NONE;

	int index = (int)thePrefs.AddCat(newcat);
	if (addTab)
		m_dlTab.InsertItem(index, newtitle);
	VerifyCatTabSize();

	return index;
}

int CTransferWnd::AddCategoryInteractive()
{
	m_nLastCatTT = -1;
	int newindex = AddCategory(_T("?"), thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR), _T(""), _T(""), false);
	CCatDialog dialog(newindex);
	if (dialog.DoModal() == IDOK) {
		theApp.emuledlg->searchwnd->UpdateCatTabs();
		m_dlTab.InsertItem(newindex, thePrefs.GetCategory(newindex)->strTitle);
		m_dlTab.SetTabTextColor(newindex, thePrefs.GetCatColor(newindex));
		EditCatTabLabel(newindex);
		thePrefs.SaveCats();
		VerifyCatTabSize();
		if (CompareDirectory(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR), thePrefs.GetCatPath(newindex)))
			theApp.emuledlg->sharedfileswnd->Reload();
		return newindex;
	}
	thePrefs.RemoveCat(newindex);
	return 0;
}

int CTransferWnd::GetTabUnderMouse(const CPoint &point)
{
	TCHITTESTINFO hitinfo;
	CRect rect;
	m_dlTab.GetWindowRect(&rect);
	hitinfo.pt = CPoint(point - rect.TopLeft());

	if (m_dlTab.GetItemRect(0, &rect) && abs(hitinfo.pt.y - rect.top) < 30)
		hitinfo.pt.y = rect.top;

	// Find the destination tab...
	return m_dlTab.HitTest(&hitinfo);
}

void CTransferWnd::OnLvnKeydownDownloadList(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	int iItem = downloadlistctrl.GetSelectionMark();
	if (iItem >= 0) {
		bool bAltKey = GetKeyState(VK_MENU) < 0;
		int iAction = EXPAND_COLLAPSE;
		if (pLVKeyDow->wVKey == VK_ADD || (bAltKey && pLVKeyDow->wVKey == VK_RIGHT))
			iAction = EXPAND_ONLY;
		else if (pLVKeyDow->wVKey == VK_SUBTRACT || (bAltKey && pLVKeyDow->wVKey == VK_LEFT))
			iAction = COLLAPSE_ONLY;
		if (iAction < EXPAND_COLLAPSE)
			downloadlistctrl.ExpandCollapseItem(iItem, iAction, true);
	}
	*pResult = 0;
}

void CTransferWnd::UpdateTabToolTips(int tab)
{
	if (tab == -1) {
		for (int i = m_tooltipCats->GetToolCount(); --i >= 0;)
			m_tooltipCats->DelTool(&m_dlTab, i);

		for (int i = 0; i < m_dlTab.GetItemCount(); ++i) {
			RECT r;
			m_dlTab.GetItemRect(i, &r);
			VERIFY(m_tooltipCats->AddTool(&m_dlTab, GetTabStatistic(i), &r, i + 1));
		}
	} else {
		RECT r;
		m_dlTab.GetItemRect(tab, &r);
		m_tooltipCats->DelTool(&m_dlTab, tab + 1);
		VERIFY(m_tooltipCats->AddTool(&m_dlTab, GetTabStatistic(tab), &r, tab + 1));
	}
}

void CTransferWnd::SetToolTipsDelay(DWORD dwDelay)
{
	m_tooltipCats->SetDelayTime(TTDT_INITIAL, dwDelay);

	CToolTipCtrl *tooltip = downloadlistctrl.GetToolTips();
	if (tooltip)
		tooltip->SetDelayTime(TTDT_INITIAL, dwDelay);

	tooltip = uploadlistctrl.GetToolTips();
	if (tooltip)
		tooltip->SetDelayTime(TTDT_INITIAL, dwDelay);
}

CString CTransferWnd::GetTabStatistic(int tab)
{
	int count = 0, dwl = 0, err = 0, paus = 0;
	float speed = 0;
	uint64 size = 0;
	uint64 trsize = 0;
	uint64 disksize = 0;

	for (POSITION pos = NULL; ;) {
		CPartFile *pFile = theApp.downloadqueue->GetFileNext(pos);
		if (pFile != NULL) {
			if (pFile->CheckShowItemInGivenCat(tab)) {
				++count;
				dwl += static_cast<int>(pFile->GetTransferringSrcCount() > 0);
				speed += pFile->GetDatarate() / 1024.0F;
				size += (uint64)pFile->GetFileSize();
				trsize += (uint64)pFile->GetCompletedSize();
				if (!pFile->IsAllocating())
					disksize += (uint64)pFile->GetRealFileSize();
				err += static_cast<int>(pFile->GetStatus() == PS_ERROR);
				paus += static_cast<int>(pFile->GetStatus() == PS_PAUSED);
			}
		}
		if (pos == NULL)
			break;
	}

	int total;
	int compl = downloadlistctrl.GetCompleteDownloads(tab, total);

	UINT uid;
	switch (thePrefs.GetCategory(tab)->prio) {
	case PR_LOW:
		uid = IDS_PRIOLOW;
		break;
	case PR_HIGH:
		uid = IDS_PRIOHIGH;
		break;
	default:
		uid = IDS_PRIONORMAL;
	}

	CString title;
	title.Format(_T("%s: %i\n\n%s: %i\n%s: %i\n%s: %i\n%s: %i\n\n%s: %s\n\n%s: %.1f %s\n%s: %s/%s\n%s%s") TOOLTIP_AUTOFORMAT_SUFFIX
		, (LPCTSTR)GetResString(IDS_FILES), count + compl
		, (LPCTSTR)GetResString(IDS_DOWNLOADING), dwl
		, (LPCTSTR)GetResString(IDS_PAUSED), paus
		, (LPCTSTR)GetResString(IDS_ERRORLIKE), err
		, (LPCTSTR)GetResString(IDS_DL_TRANSFCOMPL), compl
		, (LPCTSTR)GetResString(IDS_PRIORITY), (LPCTSTR)GetResString(uid)
		, (LPCTSTR)GetResString(IDS_DL_SPEED), speed, (LPCTSTR)GetResString(IDS_KBYTESPERSEC)
		, (LPCTSTR)GetResString(IDS_DL_SIZE), (LPCTSTR)CastItoXBytes(trsize), (LPCTSTR)CastItoXBytes(size)
		, (LPCTSTR)GetResString(IDS_ONDISK), (LPCTSTR)CastItoXBytes(disksize)
	);
	return title;
}

void CTransferWnd::OnDblClickDltab()
{
	CPoint point;
	::GetCursorPos(&point);
	int tab = GetTabUnderMouse(point);
	if (tab >= 1) {
		m_rightclickindex = tab;
		OnCommand(MP_CAT_EDIT, 0);
	}
}

void CTransferWnd::OnTabMovement(LPNMHDR, LRESULT*)
{
	UINT from = m_dlTab.GetLastMovementSource();
	UINT to = m_dlTab.GetLastMovementDestionation();

	if (from == 0 || to == 0 || from == to - 1)
		return;

	// do the reorder

	// rearrange the cat-map
	if (!thePrefs.MoveCat(from, to))
		return;

	// update partfile-stored assignment
	theApp.downloadqueue->MoveCat(from, to);

	// move category of completed files
	downloadlistctrl.MoveCompletedfilesCat(from, to);

	// of the tabcontrol itself
	m_dlTab.ReorderTab(from, to);

	UpdateCatTabTitles();
	theApp.emuledlg->searchwnd->UpdateCatTabs();

	if (to > from)
		--to;
	m_dlTab.SetCurSel(to);
	downloadlistctrl.ChangeCategory(to);
}

void CTransferWnd::VerifyCatTabSize()
{
	if (m_dwShowListIDC != IDC_DOWNLOADLIST && m_dwShowListIDC != IDC_UPLOADLIST + IDC_DOWNLOADLIST)
		return;

	int size = 0;
	for (int i = m_dlTab.GetItemCount(); --i >= 0;) {
		CRect rect;
		m_dlTab.GetItemRect(i, &rect);
		size += rect.Width();
	}

	WINDOWPLACEMENT wp;
	downloadlistctrl.GetWindowPlacement(&wp);
	int right = wp.rcNormalPosition.right;
	m_dlTab.GetWindowPlacement(&wp);
	if (wp.rcNormalPosition.right < 0)
		return;
	wp.rcNormalPosition.right = right;

	int left = wp.rcNormalPosition.right - size - 4;
	RECT rcBtn1;
	m_btnWnd1->GetWindowRect(&rcBtn1);
	ScreenToClient(&rcBtn1);
	wp.rcNormalPosition.left = (left >= rcBtn1.right + 10) ? left : rcBtn1.right + 10;

	RemoveAnchor(m_dlTab);
	m_dlTab.SetWindowPlacement(&wp);
	AddAnchor(m_dlTab, TOP_RIGHT);
}

CString CTransferWnd::GetCatTitle(int catid)
{
	static const int idscat[19] = {
		  IDS_ALL, IDS_ALLOTHERS, IDS_STATUS_NOTCOMPLETED, IDS_DL_TRANSFCOMPL, IDS_WAITING
		, IDS_DOWNLOADING, IDS_ERRORLIKE, IDS_PAUSED, IDS_SEENCOMPL, 0
		, IDS_VIDEO, IDS_AUDIO, IDS_SEARCH_ARC, IDS_SEARCH_CDIMG, IDS_SEARCH_DOC
		, IDS_SEARCH_PICS, IDS_SEARCH_PRG, 0, IDS_REGEXPRESSION};
	
	catid = (catid >= 0 && catid < 17) ? idscat[catid] : 0;
	return catid ? GetResString((UINT)catid) : CString(_T('?'));
}

void CTransferWnd::OnBnClickedChangeView()
{
	switch (m_dwShowListIDC) {
	case IDC_DOWNLOADLIST:
		ShowList(IDC_UPLOADLIST);
		break;
	case IDC_UPLOADLIST:
		ShowList(IDC_DOWNLOADCLIENTS);
		break;
	case IDC_DOWNLOADCLIENTS:
		if (!thePrefs.IsQueueListDisabled()) {
			ShowList(IDC_QUEUELIST);
			break;
		}
	case IDC_QUEUELIST:
		if (!thePrefs.IsKnownClientListDisabled()) {
			ShowList(IDC_CLIENTLIST);
			break;
		}
	case IDC_CLIENTLIST:
		ShowSplitWindow();
		break;
	case IDC_UPLOADLIST + IDC_DOWNLOADLIST:
		ShowList(IDC_DOWNLOADLIST);
	}
}

void CTransferWnd::SetWnd1Icon(EWnd1Icon iIcon)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_IMAGE;
	tbbi.iImage = iIcon;
	m_btnWnd1->SetButtonInfo((int)::GetWindowLongPtr(*m_btnWnd1, GWLP_ID), &tbbi);
}

void CTransferWnd::SetWnd2Icon(EWnd2Icon iIcon)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_IMAGE;
	tbbi.iImage = iIcon;
	m_btnWnd2->SetButtonInfo((int)::GetWindowLongPtr(*m_btnWnd2, GWLP_ID), &tbbi);
}

void CTransferWnd::ShowList(uint32 dwListIDC)
{
	RECT rcWnd;
	GetWindowRect(&rcWnd);
	ScreenToClient(&rcWnd);

	RECT rcDown;
	GetDlgItem(dwListIDC)->GetWindowRect(&rcDown);
	ScreenToClient(&rcDown);
	rcDown.top = WND1_BUTTON_YOFF + WND1_BUTTON_HEIGHT + 1;
	rcDown.bottom = rcWnd.bottom - WND1_BUTTON_HEIGHT;
	m_wndSplitter.DestroyWindow();
	RemoveAnchor(dwListIDC);
	m_btnWnd2->ShowWindow(SW_HIDE);

	m_dwShowListIDC = dwListIDC;
	uploadlistctrl.ShowWindow((m_dwShowListIDC == IDC_UPLOADLIST) ? SW_SHOW : SW_HIDE);
	queuelistctrl.ShowWindow((m_dwShowListIDC == IDC_QUEUELIST) ? SW_SHOW : SW_HIDE);
	downloadclientsctrl.ShowWindow((m_dwShowListIDC == IDC_DOWNLOADCLIENTS) ? SW_SHOW : SW_HIDE);
	clientlistctrl.ShowWindow((m_dwShowListIDC == IDC_CLIENTLIST) ? SW_SHOW : SW_HIDE);
	downloadlistctrl.ShowWindow((m_dwShowListIDC == IDC_DOWNLOADLIST) ? SW_SHOW : SW_HIDE);
	m_dlTab.ShowWindow((m_dwShowListIDC == IDC_DOWNLOADLIST) ? SW_SHOW : SW_HIDE);
	theApp.emuledlg->transferwnd->ShowToolbar(m_dwShowListIDC == IDC_DOWNLOADLIST);
	GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow((m_dwShowListIDC == IDC_QUEUELIST) ? SW_SHOW : SW_HIDE);

	switch (dwListIDC) {
	case IDC_DOWNLOADLIST:
		downloadlistctrl.MoveWindow(&rcDown);
		downloadlistctrl.ShowFilesCount();
		m_btnWnd1->CheckButton(MP_VIEW1_DOWNLOADS);
		SetWnd1Icon(w1iDownloadFiles);
		thePrefs.SetTransferWnd1(1);
		break;
	case IDC_UPLOADLIST:
		uploadlistctrl.MoveWindow(&rcDown);
		UpdateListCount(wnd2Uploading);
		m_btnWnd1->CheckButton(MP_VIEW1_UPLOADING);
		SetWnd1Icon(w1iUploading);
		thePrefs.SetTransferWnd1(2);
		break;
	case IDC_QUEUELIST:
		queuelistctrl.MoveWindow(&rcDown);
		UpdateListCount(wnd2OnQueue);
		m_btnWnd1->CheckButton(MP_VIEW1_ONQUEUE);
		SetWnd1Icon(w1iOnQueue);
		thePrefs.SetTransferWnd1(3);
		break;
	case IDC_DOWNLOADCLIENTS:
		downloadclientsctrl.MoveWindow(&rcDown);
		UpdateListCount(wnd2Downloading);
		m_btnWnd1->CheckButton(MP_VIEW1_DOWNLOADING);
		SetWnd1Icon(w1iDownloading);
		thePrefs.SetTransferWnd1(4);
		break;
	case IDC_CLIENTLIST:
		clientlistctrl.MoveWindow(&rcDown);
		UpdateListCount(wnd2Clients);
		m_btnWnd1->CheckButton(MP_VIEW1_CLIENTS);
		SetWnd1Icon(w1iClientsKnown);
		thePrefs.SetTransferWnd1(5);
		break;
	default:
		ASSERT(0);
	}
	AddAnchor(dwListIDC, TOP_LEFT, BOTTOM_RIGHT);
}

void CTransferWnd::ShowSplitWindow(bool bReDraw)
{
	thePrefs.SetTransferWnd1(0);
	m_dlTab.ShowWindow(SW_SHOW);
	if (!bReDraw && m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST)
		return;

	m_btnWnd1->CheckButton(MP_VIEW1_SPLIT_WINDOW);
	SetWnd1Icon(w1iDownloadFiles);

	CRect rcWnd;
	GetWindowRect(rcWnd);
	ScreenToClient(rcWnd);

	LONG splitpos = (thePrefs.GetSplitterbarPosition() * rcWnd.Height()) / 100;

	// do some more magic, don't ask -- just fix it.
	if (bReDraw || (m_dwShowListIDC != 0 && m_dwShowListIDC != IDC_DOWNLOADLIST + IDC_UPLOADLIST))
		splitpos += 10;

	RECT rcDown;
	downloadlistctrl.GetWindowRect(&rcDown);
	ScreenToClient(&rcDown);
	rcDown.top = WND1_BUTTON_YOFF + WND1_BUTTON_HEIGHT + 1;
	rcDown.bottom = splitpos - 5; // Magic constant '5'.
	downloadlistctrl.MoveWindow(&rcDown);

	uploadlistctrl.GetWindowRect(&rcDown);
	ScreenToClient(&rcDown);
	rcDown.right = rcWnd.right - 7;
	rcDown.bottom = rcWnd.bottom - WND1_BUTTON_HEIGHT;
	rcDown.top = splitpos + 20;

	uploadlistctrl.MoveWindow(&rcDown);
	queuelistctrl.MoveWindow(&rcDown);
	clientlistctrl.MoveWindow(&rcDown);
	downloadclientsctrl.MoveWindow(&rcDown);

	CRect rcBtn2;
	m_btnWnd2->GetWindowRect(rcBtn2);
	ScreenToClient(rcBtn2);
	RECT rcSpl;
	rcSpl.left = rcBtn2.right + 8;
	rcSpl.right = rcDown.right;
	rcSpl.top = splitpos + WND_SPLITTER_YOFF;
	rcSpl.bottom = rcSpl.top + WND_SPLITTER_HEIGHT;
	if (!m_wndSplitter) {
		m_wndSplitter.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER);
		m_wndSplitter.SetDrawBorder(true);
	} else
		m_wndSplitter.MoveWindow(&rcSpl, TRUE);
	m_btnWnd2->MoveWindow(rcBtn2.left, rcSpl.top - (WND_SPLITTER_YOFF - 1) - 5, rcBtn2.Width(), WND2_BUTTON_HEIGHT);
	DoResize(0);

	m_dwShowListIDC = IDC_DOWNLOADLIST + IDC_UPLOADLIST;
	downloadlistctrl.ShowFilesCount();
	m_btnWnd2->ShowWindow(SW_SHOW);
	theApp.emuledlg->transferwnd->ShowToolbar(true);

	RemoveAnchor(*m_btnWnd2);
	RemoveAnchor(IDC_DOWNLOADLIST);
	RemoveAnchor(IDC_UPLOADLIST);
	RemoveAnchor(IDC_QUEUELIST);
	RemoveAnchor(IDC_DOWNLOADCLIENTS);
	RemoveAnchor(IDC_CLIENTLIST);

	AddAnchor(*m_btnWnd2, ANCHOR(0, thePrefs.GetSplitterbarPosition()));
	AddAnchor(IDC_DOWNLOADLIST, TOP_LEFT, ANCHOR(100, thePrefs.GetSplitterbarPosition()));
	AddAnchor(IDC_UPLOADLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_QUEUELIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_CLIENTLIST, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	AddAnchor(IDC_DOWNLOADCLIENTS, ANCHOR(0, thePrefs.GetSplitterbarPosition()), BOTTOM_RIGHT);
	VerifyCatTabSize(); //properly position tab control

	downloadlistctrl.ShowWindow(SW_SHOW);
	uploadlistctrl.ShowWindow((m_uWnd2 == wnd2Uploading) ? SW_SHOW : SW_HIDE);
	queuelistctrl.ShowWindow((m_uWnd2 == wnd2OnQueue) ? SW_SHOW : SW_HIDE);
	downloadclientsctrl.ShowWindow((m_uWnd2 == wnd2Downloading) ? SW_SHOW : SW_HIDE);
	clientlistctrl.ShowWindow((m_uWnd2 == wnd2Clients) ? SW_SHOW : SW_HIDE);

	GetDlgItem(IDC_QUEUE_REFRESH_BUTTON)->ShowWindow((m_uWnd2 == wnd2OnQueue) ? SW_SHOW : SW_HIDE);

	UpdateListCount(m_uWnd2);
}

void CTransferWnd::OnDisableList()
{
	bool bSwitchList = false;
	if (thePrefs.m_bDisableKnownClientList) {
		clientlistctrl.DeleteAllItems();
		if (m_uWnd2 == wnd2Clients)
			bSwitchList = true;
	}
	if (thePrefs.m_bDisableQueueList) {
		queuelistctrl.DeleteAllItems();
		if (m_uWnd2 == wnd2OnQueue)
			bSwitchList = true;
	}
	if (bSwitchList)
		SwitchUploadList();
}

void CTransferWnd::OnWnd1BtnDropDown(LPNMHDR, LRESULT*)
{
	CTitleMenu menu;
	menu.CreatePopupMenu();
	menu.EnableIcons();

	menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST ? MF_GRAYED : 0), MP_VIEW1_SPLIT_WINDOW, GetResString(IDS_SPLIT_WINDOW), _T("SplitWindow"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_DOWNLOADLIST ? MF_GRAYED : 0), MP_VIEW1_DOWNLOADS, GetResString(IDS_TW_DOWNLOADS), _T("DownloadFiles"));
	menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_UPLOADLIST ? MF_GRAYED : 0), MP_VIEW1_UPLOADING, GetResString(IDS_UPLOADING), _T("Upload"));
	menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_DOWNLOADCLIENTS ? MF_GRAYED : 0), MP_VIEW1_DOWNLOADING, GetResString(IDS_DOWNLOADING), _T("Download"));
	if (!thePrefs.IsQueueListDisabled())
		menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_QUEUELIST ? MF_GRAYED : 0), MP_VIEW1_ONQUEUE, GetResString(IDS_ONQUEUE), _T("ClientsOnQueue"));
	if (!thePrefs.IsKnownClientListDisabled())
		menu.AppendMenu(MF_STRING | (m_dwShowListIDC == IDC_CLIENTLIST ? MF_GRAYED : 0), MP_VIEW1_CLIENTS, GetResString(IDS_CLIENTLIST), _T("ClientsKnown"));

	RECT rcBtn1;
	m_btnWnd1->GetWindowRect(&rcBtn1);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rcBtn1.left, rcBtn1.bottom, this);
}

void CTransferWnd::OnWnd2BtnDropDown(LPNMHDR, LRESULT*)
{
	CTitleMenu menu;
	menu.CreatePopupMenu();
	menu.EnableIcons();

	menu.AppendMenu(MF_STRING | (m_uWnd2 == wnd2Uploading ? MF_GRAYED : 0), MP_VIEW2_UPLOADING, GetResString(IDS_UPLOADING), _T("Upload"));
	menu.AppendMenu(MF_STRING | (m_uWnd2 == wnd2Downloading ? MF_GRAYED : 0), MP_VIEW2_DOWNLOADING, GetResString(IDS_DOWNLOADING), _T("Download"));
	if (!thePrefs.IsQueueListDisabled())
		menu.AppendMenu(MF_STRING | (m_uWnd2 == wnd2OnQueue ? MF_GRAYED : 0), MP_VIEW2_ONQUEUE, GetResString(IDS_ONQUEUE), _T("ClientsOnQueue"));
	if (!thePrefs.IsKnownClientListDisabled())
		menu.AppendMenu(MF_STRING | (m_uWnd2 == wnd2Clients ? MF_GRAYED : 0), MP_VIEW2_CLIENTS, GetResString(IDS_CLIENTLIST), _T("ClientsKnown"));

	RECT rcBtn2;
	m_btnWnd2->GetWindowRect(&rcBtn2);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rcBtn2.left, rcBtn2.bottom, this);
}

void CTransferWnd::ResetTransToolbar(bool bShowToolbar, bool bResetLists)
{
	if (m_btnWnd1->m_hWnd)
		RemoveAnchor(*m_btnWnd1);
	if (m_btnWnd2->m_hWnd)
		RemoveAnchor(*m_btnWnd2);

	CRect rcBtn1;
	m_btnWnd1->GetWindowRect(rcBtn1);
	ScreenToClient(rcBtn1);
	//rcBtn1.left = WND1_BUTTON_XOFF;
	rcBtn1.top = WND1_BUTTON_YOFF;
	rcBtn1.right = rcBtn1.left + WND1_BUTTON_WIDTH + (bShowToolbar ? WND1_NUM_BUTTONS * DFLT_TOOLBAR_BTN_WIDTH : 0);
	rcBtn1.bottom = rcBtn1.top + WND1_BUTTON_HEIGHT;
	m_btnWnd1->Init(!bShowToolbar);
	m_btnWnd1->MoveWindow(&rcBtn1);
	SetWnd1Icons();

	RECT rcBtn2;
	m_btnWnd2->GetWindowRect(&rcBtn2);
	ScreenToClient(&rcBtn2);
	//rcBtn2.left = WND2_BUTTON_XOFF;
	rcBtn2.right = rcBtn2.left + WND2_BUTTON_WIDTH + (bShowToolbar ? WND2_NUM_BUTTONS * DFLT_TOOLBAR_BTN_WIDTH : 0);
	rcBtn2.bottom = rcBtn2.top + WND2_BUTTON_HEIGHT;
	m_btnWnd2->Init(!bShowToolbar);
	m_btnWnd2->MoveWindow(&rcBtn2);
	SetWnd2Icons();

	if (bShowToolbar) {
		// Vista: Remove the TBSTYLE_TRANSPARENT to avoid flickering (can be done only after the toolbar was initially created with TBSTYLE_TRANSPARENT !?)
		m_btnWnd1->ModifyStyle((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0, TBSTYLE_TOOLTIPS);
		m_btnWnd1->SetExtendedStyle(m_btnWnd1->GetExtendedStyle() | TBSTYLE_EX_MIXEDBUTTONS);

		TBBUTTON atb1[1 + WND1_NUM_BUTTONS] = {};
		atb1[0].iBitmap = w1iDownloadFiles;
		atb1[0].idCommand = IDC_DOWNLOAD_ICO;
		atb1[0].fsState = TBSTATE_ENABLED;
		atb1[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
		atb1[0].iString = -1;

		atb1[1].iBitmap = w1iSplitWindow;
		atb1[1].idCommand = MP_VIEW1_SPLIT_WINDOW;
		atb1[1].fsState = TBSTATE_ENABLED;
		atb1[1].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[1].iString = -1;

		atb1[2].iBitmap = w1iDownloadFiles;
		atb1[2].idCommand = MP_VIEW1_DOWNLOADS;
		atb1[2].fsState = TBSTATE_ENABLED;
		atb1[2].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[2].iString = -1;

		atb1[3].iBitmap = w1iUploading;
		atb1[3].idCommand = MP_VIEW1_UPLOADING;
		atb1[3].fsState = TBSTATE_ENABLED;
		atb1[3].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[3].iString = -1;

		atb1[4].iBitmap = w1iDownloading;
		atb1[4].idCommand = MP_VIEW1_DOWNLOADING;
		atb1[4].fsState = TBSTATE_ENABLED;
		atb1[4].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[4].iString = -1;

		atb1[5].iBitmap = w1iOnQueue;
		atb1[5].idCommand = MP_VIEW1_ONQUEUE;
		atb1[5].fsState = thePrefs.IsQueueListDisabled() ? 0 : TBSTATE_ENABLED;
		atb1[5].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[5].iString = -1;

		atb1[6].iBitmap = w1iClientsKnown;
		atb1[6].idCommand = MP_VIEW1_CLIENTS;
		atb1[6].fsState = thePrefs.IsKnownClientListDisabled() ? 0 : TBSTATE_ENABLED;
		atb1[6].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb1[6].iString = -1;
		m_btnWnd1->AddButtons(_countof(atb1), atb1);

		TBBUTTONINFO tbbi = {};
		tbbi.cbSize = (UINT)sizeof tbbi;
		tbbi.dwMask = TBIF_SIZE | TBIF_BYINDEX;
		tbbi.cx = WND1_BUTTON_WIDTH;
		m_btnWnd1->SetButtonInfo(0, &tbbi);

		// 'GetMaxSize' does not work properly under:
		//	- Win98SE with COMCTL32 v5.80
		//	- Win2000 with COMCTL32 v5.81
		// The value returned by 'GetMaxSize' is just couple of pixels too small so that the
		// last toolbar button is nearly not visible at all.
		// So, to circumvent such problems, the toolbar control should be created right with
		// the needed size so that we do not really need to call the 'GetMaxSize' function.
		// Although it would be better to call it to adapt for system metrics basically.
		if (theApp.m_ullComCtrlVer > MAKEDLLVERULL(5, 81, 0, 0)) {
			CSize size;
			m_btnWnd1->GetMaxSize(&size);
			m_btnWnd1->GetWindowRect(&rcBtn1);
			ScreenToClient(&rcBtn1);
			// the with of the toolbar should already match the needed size (see comment above)
			ASSERT(size.cx == rcBtn1.Width());
			m_btnWnd1->MoveWindow(rcBtn1.left, rcBtn1.top, size.cx, rcBtn1.Height());
		}
		/*---*/
				// Vista: Remove the TBSTYLE_TRANSPARENT to avoid flickering (can be done only after the toolbar was initially created with TBSTYLE_TRANSPARENT !?)
		m_btnWnd2->ModifyStyle((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0, TBSTYLE_TOOLTIPS);
		m_btnWnd2->SetExtendedStyle(m_btnWnd2->GetExtendedStyle() | TBSTYLE_EX_MIXEDBUTTONS);

		TBBUTTON atb2[1 + WND2_NUM_BUTTONS] = {};
		atb2[0].iBitmap = w2iUploading;
		atb2[0].idCommand = IDC_UPLOAD_ICO;
		atb2[0].fsState = TBSTATE_ENABLED;
		atb2[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
		atb2[0].iString = -1;

		atb2[1].iBitmap = w2iUploading;
		atb2[1].idCommand = MP_VIEW2_UPLOADING;
		atb2[1].fsState = TBSTATE_ENABLED;
		atb2[1].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb2[1].iString = -1;

		atb2[2].iBitmap = w2iDownloading;
		atb2[2].idCommand = MP_VIEW2_DOWNLOADING;
		atb2[2].fsState = TBSTATE_ENABLED;
		atb2[2].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb2[2].iString = -1;

		atb2[3].iBitmap = w2iOnQueue;
		atb2[3].idCommand = MP_VIEW2_ONQUEUE;
		atb2[3].fsState = thePrefs.IsQueueListDisabled() ? 0 : TBSTATE_ENABLED;
		atb2[3].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb2[3].iString = -1;

		atb2[4].iBitmap = w2iClientsKnown;
		atb2[4].idCommand = MP_VIEW2_CLIENTS;
		atb2[4].fsState = thePrefs.IsKnownClientListDisabled() ? 0 : TBSTATE_ENABLED;
		atb2[4].fsStyle = BTNS_BUTTON | BTNS_CHECKGROUP | BTNS_AUTOSIZE;
		atb2[4].iString = -1;
		m_btnWnd2->AddButtons(_countof(atb2), atb2);

		memset(&tbbi, 0, sizeof tbbi);
		tbbi.cbSize = (UINT)sizeof tbbi;
		tbbi.dwMask = TBIF_SIZE | TBIF_BYINDEX;
		tbbi.cx = WND2_BUTTON_WIDTH;
		m_btnWnd2->SetButtonInfo(0, &tbbi);

		// 'GetMaxSize' does not work properly under:
		//	- Win98SE with COMCTL32 v5.80
		//	- Win2000 with COMCTL32 v5.81
		// The value returned by 'GetMaxSize' is just couple of pixels too small so that the
		// last toolbar button is nearly not visible at all.
		// So, to circumvent such problems, the toolbar control should be created right with
		// the needed size so that we do not really need to call the 'GetMaxSize' function.
		// Although it would be better to call it to adapt for system metrics basically.
		if (theApp.m_ullComCtrlVer > MAKEDLLVERULL(5, 81, 0, 0)) {
			SIZE size;
			m_btnWnd2->GetMaxSize(&size);
			CRect rc;
			m_btnWnd2->GetWindowRect(&rc);
			ScreenToClient(&rc);
			// the with of the toolbar should already match the needed size (see comment above)
			ASSERT(size.cx == rc.Width());
			m_btnWnd2->MoveWindow(rc.left, rc.top, size.cx, rc.Height());
		}
	} else {
		// Vista: Remove the TBSTYLE_TRANSPARENT to avoid flickering (can be done only after the toolbar was initially created with TBSTYLE_TRANSPARENT !?)
		m_btnWnd1->ModifyStyle(TBSTYLE_TOOLTIPS | ((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0), 0);
		m_btnWnd1->SetExtendedStyle(m_btnWnd1->GetExtendedStyle() & ~TBSTYLE_EX_MIXEDBUTTONS);
		m_btnWnd1->RecalcLayout(true);

		// Vista: Remove the TBSTYLE_TRANSPARENT to avoid flickering (can be done only after the toolbar was initially created with TBSTYLE_TRANSPARENT !?)
		m_btnWnd2->ModifyStyle(TBSTYLE_TOOLTIPS | ((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0), 0);
		m_btnWnd2->SetExtendedStyle(m_btnWnd2->GetExtendedStyle() & ~TBSTYLE_EX_MIXEDBUTTONS);
		m_btnWnd2->RecalcLayout(true);
	}

	AddAnchor(*m_btnWnd1, TOP_LEFT);
	AddAnchor(*m_btnWnd2, ANCHOR(0, thePrefs.GetSplitterbarPosition()));

	if (bResetLists) {
		LocalizeToolbars();
		ShowSplitWindow(true);
		ShowWnd2(m_uWnd2);
	}
}

BOOL CTransferWnd::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_Transfers);
	return TRUE;
}

HBRUSH CTransferWnd::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = theApp.emuledlg->GetCtlColor(pDC, pWnd, nCtlColor);
	return hbr ? hbr : __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CTransferWnd::OnPaint()
{
	CResizableFormView::OnPaint();
	CRect rcWnd;
	GetWindowRect(rcWnd);

	// Another small work around: Init/Redraw the layout as soon as we have our real windows size
	// as the initial size is far below the minimum and will mess things up which expect this size
	if (!m_bLayoutInited && rcWnd.Height() > 400) {
		m_bLayoutInited = true;
		if (m_dwShowListIDC == IDC_DOWNLOADLIST + IDC_UPLOADLIST)
			ShowSplitWindow(true);
		else
			ShowList(m_dwShowListIDC);
	}

	if (m_wndSplitter) {
		if (rcWnd.Height() > 0) {
			RECT rcDown;
			downloadlistctrl.GetWindowRect(&rcDown);
			ScreenToClient(&rcDown);

			RECT rcBtn2;
			m_btnWnd2->GetWindowRect(&rcBtn2);
			ScreenToClient(&rcBtn2);

			// splitter paint update
			RECT rcSpl;
			rcSpl.left = rcBtn2.right + 8;
			rcSpl.right = rcDown.right;
			rcSpl.top = rcDown.bottom + WND_SPLITTER_YOFF;
			rcSpl.bottom = rcSpl.top + WND_SPLITTER_HEIGHT;
			m_wndSplitter.MoveWindow(&rcSpl, TRUE);
			UpdateSplitterRange();
		}
	}

	// Workaround to solve a glitch with WM_SETTINGCHANGE message
	if (m_btnWnd1->m_hWnd && m_btnWnd1->GetBtnWidth(IDC_DOWNLOAD_ICO) != WND1_BUTTON_WIDTH)
		m_btnWnd1->SetBtnWidth(IDC_DOWNLOAD_ICO, WND1_BUTTON_WIDTH);
	if (m_btnWnd2->m_hWnd && m_btnWnd2->GetBtnWidth(IDC_UPLOAD_ICO) != WND2_BUTTON_WIDTH)
		m_btnWnd2->SetBtnWidth(IDC_UPLOAD_ICO, WND2_BUTTON_WIDTH);
}

void CTransferWnd::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_KEYMENU) {
		if (lParam == EMULE_HOTMENU_ACCEL)
			theApp.emuledlg->SendMessage(WM_COMMAND, IDC_HOTMENU);
		else
			theApp.emuledlg->SendMessage(WM_SYSCOMMAND, nID, lParam);
	} else
		__super::OnSysCommand(nID, lParam);
}