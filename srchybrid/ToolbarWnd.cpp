//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "toolbarwnd.h"
#include "HelpIDs.h"
#include "OtherFunctions.h"
#include "ToolBarCtrlX.h"
#include "MenuCmds.h"
#include "DownloadListCtrl.h"
#include "TransferDlg.h"
#include "Preferences.h"
#include "Otherfunctions.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CToolbarWnd, CDialogBar);

BEGIN_MESSAGE_MAP(CToolbarWnd, CDialogBar)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_SYSCOLORCHANGE()
	ON_MESSAGE(WM_INITDIALOG, OnInitDialog)
	ON_WM_SETCURSOR()
	ON_WM_HELPINFO()
	ON_NOTIFY(TBN_DROPDOWN, IDC_DTOOLBAR, OnBtnDropDown)
	ON_WM_SYSCOMMAND()
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()


CToolbarWnd::CToolbarWnd()
	: m_pCommandTargetWnd()
{
	m_btnBar = new CToolBarCtrlX;
	// load default windows system cursor (a shared resource)
	m_hcurMove = ::LoadCursor(NULL, IDC_SIZEALL);
}

CToolbarWnd::~CToolbarWnd()
{
	delete m_btnBar;
}

void CToolbarWnd::DoDataExchange(CDataExchange *pDX)
{
	CDialogBar::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DTOOLBAR, *m_btnBar);
}

#define DTOOLBAR_NUM_BUTTONS 18
void CToolbarWnd::FillToolbar()
{
	m_btnBar->DeleteAllButtons();

	TBBUTTON atb1[DTOOLBAR_NUM_BUTTONS] = {};

	atb1[0].iBitmap = 0;
	atb1[0].idCommand = MP_PRIOLOW;
	atb1[0].fsState = TBSTATE_WRAP;
	atb1[0].fsStyle = BTNS_DROPDOWN | BTNS_AUTOSIZE;
	CString sPrio(GetResString(IDS_PRIORITY));
	sPrio.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_DOWNLOAD));
	atb1[0].iString = m_btnBar->AddString(sPrio);

	atb1[1].iBitmap = 1;
	atb1[1].idCommand = MP_PAUSE;
	atb1[1].fsState = TBSTATE_WRAP;
	atb1[1].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[1].iString = m_btnBar->AddString(GetResString(IDS_DL_PAUSE));

	atb1[2].iBitmap = 2;
	atb1[2].idCommand = MP_STOP;
	atb1[2].fsState = TBSTATE_WRAP;
	atb1[2].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[2].iString = m_btnBar->AddString(GetResString(IDS_DL_STOP));

	atb1[3].iBitmap = 3;
	atb1[3].idCommand = MP_RESUME;
	atb1[3].fsState = TBSTATE_WRAP;
	atb1[3].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[3].iString = m_btnBar->AddString(GetResString(IDS_DL_RESUME));

	atb1[4].iBitmap = 4;
	atb1[4].idCommand = MP_CANCEL;
	atb1[4].fsState = TBSTATE_WRAP;
	atb1[4].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[4].iString = m_btnBar->AddString(GetResString(IDS_MAIN_BTN_CANCEL));
	/////////////
	atb1[5].iBitmap = -1;
	atb1[5].idCommand = 0;
	atb1[5].fsState = TBSTATE_WRAP;
	atb1[5].fsStyle = BTNS_SEP;
	atb1[5].iString = -1;

	atb1[6].iBitmap = 5;
	atb1[6].idCommand = MP_OPEN;
	atb1[6].fsState = TBSTATE_WRAP;
	atb1[6].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[6].iString = m_btnBar->AddString(GetResString(IDS_DL_OPEN));

	atb1[7].iBitmap = 6;
	atb1[7].idCommand = MP_OPENFOLDER;
	atb1[7].fsState = TBSTATE_WRAP;
	atb1[7].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[7].iString = m_btnBar->AddString(GetResString(IDS_OPENFOLDER));

	atb1[8].iBitmap = 7;
	atb1[8].idCommand = MP_PREVIEW;
	atb1[8].fsState = TBSTATE_WRAP;
	atb1[8].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[8].iString = m_btnBar->AddString(GetResString(IDS_DL_PREVIEW));

	atb1[9].iBitmap = 8;
	atb1[9].idCommand = MP_METINFO;
	atb1[9].fsState = TBSTATE_WRAP;
	atb1[9].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[9].iString = m_btnBar->AddString(GetResString(IDS_DL_INFO));

	atb1[10].iBitmap = 9;
	atb1[10].idCommand = MP_VIEWFILECOMMENTS;
	atb1[10].fsState = TBSTATE_WRAP;
	atb1[10].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[10].iString = m_btnBar->AddString(GetResString(IDS_CMT_SHOWALL));

	atb1[11].iBitmap = 10;
	atb1[11].idCommand = MP_SHOWED2KLINK;
	atb1[11].fsState = TBSTATE_WRAP;
	atb1[11].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[11].iString = m_btnBar->AddString(GetResString(IDS_DL_SHOWED2KLINK));

	/////////////
	atb1[12].iBitmap = -1;
	atb1[12].idCommand = 0;
	atb1[12].fsState = TBSTATE_WRAP;
	atb1[12].fsStyle = BTNS_SEP;
	atb1[12].iString = -1;

	atb1[13].iBitmap = 11;
	atb1[13].idCommand = MP_NEWCAT;
	atb1[13].fsState = TBSTATE_WRAP;
	atb1[13].fsStyle = BTNS_DROPDOWN | BTNS_AUTOSIZE;
	atb1[13].iString = m_btnBar->AddString(GetResString(IDS_TOCAT));

	atb1[14].iBitmap = 12;
	atb1[14].idCommand = MP_CLEARCOMPLETED;
	atb1[14].fsState = TBSTATE_WRAP;
	atb1[14].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[14].iString = m_btnBar->AddString(GetResString(IDS_DL_CLEAR));

	atb1[15].iBitmap = 13;
	atb1[15].idCommand = MP_SEARCHRELATED;
	atb1[15].fsState = TBSTATE_WRAP;
	atb1[15].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[15].iString = m_btnBar->AddString(GetResString(IDS_SEARCHRELATED));

	/////////////
	atb1[16].iBitmap = -1;
	atb1[16].idCommand = 0;
	atb1[16].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
	atb1[16].fsStyle = BTNS_SEP;
	atb1[16].iString = -1;

	atb1[17].iBitmap = 14;
	atb1[17].idCommand = MP_FIND;
	atb1[17].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
	atb1[17].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb1[17].iString = m_btnBar->AddString(GetResString(IDS_FIND));

	m_btnBar->AddButtons(_countof(atb1), atb1);
}

LRESULT CToolbarWnd::OnInitDialog(WPARAM, LPARAM)
{
	Default();
	InitWindowStyles(this);

	//(void)m_sizeDefault; // not yet set
	CRect sizeDefault;
	GetWindowRect(&sizeDefault);
	static const RECT rcBorders = { 4, 4, 4, 4 };
	SetBorders(&rcBorders);
	m_szFloat.cx = sizeDefault.Width() + rcBorders.left + rcBorders.right + ::GetSystemMetrics(SM_CXEDGE) * 2;
	m_szFloat.cy = sizeDefault.Height() + rcBorders.top + rcBorders.bottom + ::GetSystemMetrics(SM_CYEDGE) * 2;
	m_szMRU = m_szFloat;
	UpdateData(FALSE);

	// Initialize the toolbar
	CImageList iml;
	int nFlags = theApp.m_iDfltImageListColorFlags;
	// older Windows versions image lists cannot create monochrome (disabled) icons which have alpha support
	// so we have to take care of this ourself
	bool bNeedMonoIcons = thePrefs.GetWindowsVersion() < _WINVER_VISTA_ && nFlags != ILC_COLOR4;
	nFlags |= ILC_MASK;
	iml.Create(16, 16, nFlags, 1, 1);
	iml.Add(CTempIconLoader(_T("FILEPRIORITY")));
	iml.Add(CTempIconLoader(_T("PAUSE")));
	iml.Add(CTempIconLoader(_T("STOP")));
	iml.Add(CTempIconLoader(_T("RESUME")));
	iml.Add(CTempIconLoader(_T("DELETE")));
	iml.Add(CTempIconLoader(_T("OPENFILE")));
	iml.Add(CTempIconLoader(_T("OPENFOLDER")));
	iml.Add(CTempIconLoader(_T("PREVIEW")));
	iml.Add(CTempIconLoader(_T("FILEINFO")));
	iml.Add(CTempIconLoader(_T("FILECOMMENTS")));
	iml.Add(CTempIconLoader(_T("ED2KLINK")));
	iml.Add(CTempIconLoader(_T("CATEGORY")));
	iml.Add(CTempIconLoader(_T("CLEARCOMPLETE")));
	iml.Add(CTempIconLoader(_T("KadFileSearch")));
	iml.Add(CTempIconLoader(_T("Search")));

	if (bNeedMonoIcons) {
		CImageList iml2;
		iml2.Create(16, 16, nFlags, 1, 1);
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("FILEPRIORITY"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("PAUSE"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("STOP"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("RESUME"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("DELETE"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("OPENFILE"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("OPENFOLDER"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("PREVIEW"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("FILEINFO"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("FILECOMMENTS"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("ED2KLINK"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("CATEGORY"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("CLEARCOMPLETE"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("KadFileSearch"))));
		VERIFY(AddIconGrayscaledToImageList(iml2, CTempIconLoader(_T("Search"))));
		CImageList *pImlOld = m_btnBar->SetDisabledImageList(&iml2);
		iml2.Detach();
		if (pImlOld)
			pImlOld->DeleteImageList();
	}
	CImageList *pImlOld = m_btnBar->SetImageList(&iml);
	iml.Detach();
	if (pImlOld)
		pImlOld->DeleteImageList();

	m_btnBar->ModifyStyle((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0, 0);
	m_btnBar->SetMaxTextRows(0);

	FillToolbar();

	return TRUE;
}

#define	MIN_HORZ_WIDTH	200
#define	MIN_VERT_WIDTH	36

CSize CToolbarWnd::CalcDynamicLayout(int nLength, DWORD dwMode)
{
	CFrameWnd *pFrm = GetDockingFrame();

	// This function is typically called with
	// CSize sizeHorz = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK);
	// CSize sizeVert = m_pBar->CalcDynamicLayout(0, LM_VERTDOCK);
	// CSize sizeFloat = m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH);

	CRect rcFrmClnt;
	pFrm->GetClientRect(&rcFrmClnt);
	CRect rcInside(rcFrmClnt);
	CalcInsideRect(rcInside, dwMode & LM_HORZDOCK);
	RECT rcBorders =
	{
		rcInside.left - rcFrmClnt.left,
		rcInside.top - rcFrmClnt.top,
		rcFrmClnt.right - rcInside.right,
		rcFrmClnt.bottom - rcInside.bottom
	};

	if (dwMode & (LM_HORZDOCK | LM_VERTDOCK)) {
		if (dwMode & LM_VERTDOCK) {
			CSize szFloat;
			szFloat.cx = MIN_VERT_WIDTH;
			szFloat.cy = rcFrmClnt.Height() + ::GetSystemMetrics(SM_CYEDGE) * 2;
			m_szFloat = szFloat;
			return szFloat;
		}
		if (dwMode & LM_HORZDOCK) {
			CSize szFloat;
			szFloat.cx = rcFrmClnt.Width() + ::GetSystemMetrics(SM_CXEDGE) * 2;
			szFloat.cy = m_sizeDefault.cy + rcBorders.top + rcBorders.bottom;
			m_szFloat = szFloat;
			return szFloat;
		}
		return CDialogBar::CalcDynamicLayout(nLength, dwMode);
	}

	if (dwMode & LM_MRUWIDTH)
		return m_szMRU;

	if (dwMode & LM_COMMIT) {
		m_szMRU = m_szFloat;
		return m_szFloat;
	}

	CSize szFloat;
	if ((dwMode & LM_LENGTHY) == 0) {
		szFloat.cx = nLength;
		if (nLength < m_sizeDefault.cx + rcBorders.left + rcBorders.right) {
			szFloat.cx = MIN_VERT_WIDTH;
			szFloat.cy = MIN_HORZ_WIDTH;
		} else
			szFloat.cy = m_sizeDefault.cy + rcBorders.top + rcBorders.bottom;
	} else {
		szFloat.cy = nLength;
		if (nLength < MIN_HORZ_WIDTH) {
			szFloat.cx = m_sizeDefault.cx + rcBorders.left + rcBorders.right;
			szFloat.cy = m_sizeDefault.cy + rcBorders.top + rcBorders.bottom;
		} else
			szFloat.cx = MIN_VERT_WIDTH;
	}

	m_szFloat = szFloat;
	return szFloat;
}

BOOL CToolbarWnd::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	if (m_hcurMove && ((m_dwStyle & (CBRS_GRIPPER | CBRS_FLOATING)) == CBRS_GRIPPER) && pWnd->GetSafeHwnd() == m_hWnd) {
		CPoint ptCursor;
		if (::GetCursorPos(&ptCursor)) {
			ScreenToClient(&ptCursor);
			CRect rcClnt;
			GetClientRect(&rcClnt);
			if (rcClnt.PtInRect(ptCursor))
				if ((m_dwStyle & CBRS_ORIENT_HORZ ? ptCursor.x : ptCursor.y) <= 10) {
					::SetCursor(m_hcurMove); //mouse over the gripper
					return TRUE;
				}
		}
	}
	return CDialogBar::OnSetCursor(pWnd, nHitTest, message);
}

void CToolbarWnd::OnSize(UINT nType, int cx, int cy)
{
	CDialogBar::OnSize(nType, cx, cy);
	if (m_btnBar->m_hWnd == 0)
		return;

	if (cx >= MIN_HORZ_WIDTH) {
		CRect rcClient;
		GetClientRect(&rcClient);
		CalcInsideRect(rcClient, TRUE);
		m_btnBar->MoveWindow(rcClient.left + 1, rcClient.top, rcClient.Width() - 8, 22);
		//int iWidthOpts = rcClient.right - (rcClient.left + m_rcOpts.left);
		/*HDWP hdwp = BeginDeferWindowPos(0);
		if (hdwp) {
			UINT uFlags = SWP_NOZORDER | SWP_NOACTIVATE;
			//hdwp = DeferWindowPos(hdwp, *GetDlgItem(IDC_MSTATIC3), NULL, rcClient.left + m_rcNameLbl.left, rcClient.top + m_rcNameLbl.top, m_rcNameLbl.Width(), m_rcNameLbl.Height(), uFlags);
			VERIFY( EndDeferWindowPos(hdwp) );
		}*/
	} else if (cx < MIN_HORZ_WIDTH) {
		CRect rcClient;
		GetClientRect(&rcClient);
		CalcInsideRect(rcClient, FALSE);
		m_btnBar->MoveWindow(rcClient.left, rcClient.top + 1, 24, rcClient.Height() - 1);
	}
}

void CToolbarWnd::OnUpdateCmdUI(CFrameWnd* /*pTarget*/, BOOL /*bDisableIfNoHndler*/)
{
	if (!theApp.IsClosing()	&& m_pCommandTargetWnd != NULL) {
		CList<int> liCommands;
		if (m_pCommandTargetWnd->ReportAvailableCommands(liCommands))
			OnAvailableCommandsChanged(&liCommands);
	}
	// Disable MFC's command routing by not passing the message flow to the base class
}

void CToolbarWnd::OnDestroy()
{
	CDialogBar::OnDestroy();
}

void CToolbarWnd::OnSysColorChange()
{
	CDialogBar::OnSysColorChange();
	//SetAllIcons();
}

void CToolbarWnd::Localize()
{
	SetWindowText(GetResString(IDS_DOWNLOADCOMMANDS));
	FillToolbar();
}

BOOL CToolbarWnd::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
		return FALSE;
	return CDialogBar::PreTranslateMessage(pMsg);
}

BOOL CToolbarWnd::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_Transfers);
	return TRUE;
}

void CToolbarWnd::OnAvailableCommandsChanged(CList<int> *liCommands)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;

	for (int i = m_btnBar->GetButtonCount(); --i >= 0;) {
		tbbi.dwMask = TBIF_COMMAND | TBIF_BYINDEX | TBIF_STATE | TBIF_STYLE;
		m_btnBar->GetButtonInfo(i, &tbbi);
		if ((tbbi.fsStyle & BTNS_SEP) == 0)
			m_btnBar->EnableButton(tbbi.idCommand, static_cast<BOOL>(liCommands->Find(tbbi.idCommand) != NULL));
	}
}

BOOL CToolbarWnd::OnCommand(WPARAM wParam, LPARAM)
{
	if (LOWORD(wParam) == MP_TOGGLEDTOOLBAR) {
		theApp.emuledlg->transferwnd->ShowToolbar(false);
		thePrefs.SetDownloadToolbar(false);
	} else if (m_pCommandTargetWnd != 0)
		m_pCommandTargetWnd->SendMessage(WM_COMMAND, wParam, 0);
	return TRUE;
}

void CToolbarWnd::OnBtnDropDown(LPNMHDR pNMHDR, LRESULT *pResult)
{
	TBNOTIFY *tbn = (TBNOTIFY*)pNMHDR;
	if (tbn->iItem == MP_PRIOLOW) {
		RECT rc;
		m_btnBar->GetItemRect(m_btnBar->CommandToIndex(MP_PRIOLOW), &rc);
		m_btnBar->ClientToScreen(&rc);
		m_pCommandTargetWnd->GetPrioMenu()->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rc.left, rc.bottom, this);
	} else if (tbn->iItem == MP_NEWCAT) {
		RECT rc;
		m_btnBar->GetItemRect(m_btnBar->CommandToIndex(MP_NEWCAT), &rc);
		m_btnBar->ClientToScreen(&rc);
		CMenu menu;
		menu.CreatePopupMenu();
		m_pCommandTargetWnd->FillCatsMenu(menu);
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rc.left, rc.bottom, this);
	} else
		ASSERT(0);
	*pResult = TBDDRET_DEFAULT;
}

void CToolbarWnd::OnContextMenu(CWnd*, CPoint point)
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, MP_TOGGLEDTOOLBAR, GetResString(IDS_CLOSETOOLBAR));
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CToolbarWnd::DelayShow(BOOL bShow)
{
	// Yes, it is somewhat ugly but still the best way (without partially rewriting 3 MFC classes)
	// to know if the user clicked on the Close-Button of our floating Bar
	if (!bShow && m_pDockSite != NULL && m_pDockBar != NULL) {
		if (m_pDockBar->m_bFloating) {
			CWnd *pDockFrame = m_pDockBar->GetParent();
			ASSERT(pDockFrame != NULL);
			if (pDockFrame != NULL) {
				CPoint point;
				::GetCursorPos(&point);
				LRESULT res = pDockFrame->SendMessage(WM_NCHITTEST, 0, MAKELONG(point.x, point.y));
				if (res == HTCLOSE)
					thePrefs.SetDownloadToolbar(false);
			}
		}
	}
	__super::DelayShow(bShow);
}

void CToolbarWnd::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_KEYMENU) {
		if (lParam == EMULE_HOTMENU_ACCEL)
			theApp.emuledlg->SendMessage(WM_COMMAND, IDC_HOTMENU);
		else
			theApp.emuledlg->SendMessage(WM_SYSCOMMAND, nID, lParam);
		return;
	}
	__super::OnSysCommand(nID, lParam);
}