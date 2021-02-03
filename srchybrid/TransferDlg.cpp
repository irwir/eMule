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
#include "emuleDlg.h"
#include "TransferDlg.h"
#include "TransferWnd.h"
#include "ToolbarWnd.h"
#include "OtherFunctions.h"
#include "HelpIDs.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	IDBAR_DOWNLOAD_TOOLBAR		(AFX_IDW_CONTROLBAR_FIRST + 32 + 2)	// do NOT change that ID, if not absolutely needed (it is stored by MFC in the bar profile!)
#define	DOWNLOAD_TOOLBAR_PROFILE	_T("DownloadFrmBarState")

IMPLEMENT_DYNCREATE(CTransferDlg, CFrameWnd)

BEGIN_MESSAGE_MAP(CTransferDlg, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SHOWWINDOW()
	ON_WM_SETFOCUS()
	ON_WM_CLOSE()
	ON_WM_SYSCOMMAND()
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CTransferDlg::CTransferDlg()
{
	m_pwndToolbar = new CToolbarWnd;
	m_pwndTransfer = NULL;
}

CTransferDlg::~CTransferDlg()
{
	delete m_pwndToolbar;
}

BOOL CTransferDlg::Create(CWnd *pParent)
{
	// *) The initial size of that frame window must not exceed the window size of the
	//    dialog resource template of the client window (the search results window).
	// *) The dialog resource template's window size (of the search results window) must not
	//	  exceed the minimum client area of the frame window.
	// Otherwise we may get scrollbars in the search results window
	static const RECT rc = { 0, 0, 50, 50 };
	return CFrameWnd::Create(NULL, _T("Transfer"), WS_CHILD | WS_CLIPCHILDREN, rc, pParent, NULL, 0, NULL);
}

int CTransferDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CCreateContext context;
	context.m_pCurrentFrame = this;
	context.m_pCurrentDoc = NULL;
	context.m_pNewViewClass = RUNTIME_CLASS(CTransferWnd);
	context.m_pNewDocTemplate = NULL;
	m_pwndTransfer = static_cast<CTransferWnd*>(CreateView(&context));
	m_pwndTransfer->ModifyStyle(WS_BORDER, 0);
	m_pwndTransfer->ModifyStyleEx(WS_EX_CLIENTEDGE, WS_EX_STATICEDGE);

	m_pwndToolbar->Create(this, IDD_DOWNLOAD_TOOLBARS
		, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_SIZE_FIXED | CBRS_SIZE_DYNAMIC | CBRS_GRIPPER
		, IDBAR_DOWNLOAD_TOOLBAR);
	ASSERT(m_pwndToolbar->GetStyle() & WS_CLIPSIBLINGS);
	ASSERT(m_pwndToolbar->GetStyle() & WS_CLIPCHILDREN);
	m_pwndToolbar->SetWindowText(GetResString(IDS_DOWNLOADCOMMANDS));
	m_pwndToolbar->EnableDocking(CBRS_ALIGN_ANY);

	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(m_pwndToolbar, AFX_IDW_DOCKBAR_LEFT, (LPRECT)NULL);

	m_pwndTransfer->SendMessage(WM_INITIALUPDATE);

	LoadBarState(DOWNLOAD_TOOLBAR_PROFILE);
	DockToolbarWnd(); // Too much bug reports about vanished search parameters window. Force to dock.
	ShowToolbar(thePrefs.IsDownloadToolbarEnabled());
	m_pwndToolbar->SetCommandTargetWnd(GetDownloadList());
	Localize();

	return 0;
}

void CTransferDlg::OnClose()
{
	SaveBarState(DOWNLOAD_TOOLBAR_PROFILE);
	CFrameWnd::OnClose();
}

void CTransferDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CFrameWnd::OnShowWindow(bShow, nStatus);
	if (m_pwndToolbar->IsFloating()) {
		//ShowControlBar(m_pwndParams, bShow, TRUE);
		DockToolbarWnd(); // Too much bug reports about vanished search parameters window. Force to dock.
	}
}

void CTransferDlg::OnSetFocus(CWnd *pOldWnd)
{
	CFrameWnd::OnSetFocus(pOldWnd);
}

void CTransferDlg::DockToolbarWnd()
{
	if (m_pwndToolbar->IsFloating()) {
		UINT uMRUDockID = AFX_IDW_DOCKBAR_TOP;
		if (m_pwndToolbar->m_pDockContext)
			uMRUDockID = m_pwndToolbar->m_pDockContext->m_uMRUDockID;
		DockControlBar(m_pwndToolbar, uMRUDockID);
	}
}

void CTransferDlg::Localize()
{
	m_pwndTransfer->Localize();
	m_pwndToolbar->Localize();
}

void CTransferDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_KEYMENU) {
		if (lParam == EMULE_HOTMENU_ACCEL)
			theApp.emuledlg->SendMessage(WM_COMMAND, IDC_HOTMENU);
		else
			theApp.emuledlg->SendMessage(WM_SYSCOMMAND, nID, lParam);
		return;
	}
	CFrameWnd::OnSysCommand(nID, lParam);
}

BOOL CTransferDlg::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		// Don't handle Ctrl+Tab in this window. It will be handled by main window.
		if (pMsg->wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0) {
			// UGLY: Because this window is a 'CFrameWnd' (rather than a 'CDialog' like
			// the other eMule main windows) we can not use MFC's message routing. Need
			// to explicitly send that message to the main window.
			theApp.emuledlg->PostMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
			return TRUE;
		}
	}
	return CFrameWnd::PreTranslateMessage(pMsg);
}

BOOL CTransferDlg::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_Transfers);
	return TRUE;
}

void CTransferDlg::ShowToolbar(bool bShow)
{
	if (m_pwndToolbar->IsVisible() != static_cast<BOOL>(bShow))
		if (thePrefs.IsDownloadToolbarEnabled() || !bShow)
			ShowControlBar(m_pwndToolbar, static_cast<BOOL>(bShow), TRUE);
}



//////////////////////////////////////////////////////////////////////
// Wrappers

void CTransferDlg::ShowQueueCount(INT_PTR number)
{
	m_pwndTransfer->ShowQueueCount(number);
}

void CTransferDlg::UpdateFilesCount(int iCount)
{
	m_pwndTransfer->UpdateFilesCount(iCount);
}

void CTransferDlg::UpdateCatTabTitles(bool force)
{
	m_pwndTransfer->UpdateCatTabTitles(force);
}

void CTransferDlg::VerifyCatTabSize()
{
	m_pwndTransfer->VerifyCatTabSize();
}

int CTransferDlg::AddCategoryInteractive()
{
	return m_pwndTransfer->AddCategoryInteractive();
}

void CTransferDlg::SwitchUploadList()
{
	m_pwndTransfer->SwitchUploadList();
}

void CTransferDlg::ResetTransToolbar(bool bShowToolbar, bool bResetLists)
{
	m_pwndTransfer->ResetTransToolbar(bShowToolbar, bResetLists);
}

void CTransferDlg::SetToolTipsDelay(DWORD dwDelay)
{
	m_pwndTransfer->SetToolTipsDelay(dwDelay);
}

void CTransferDlg::OnDisableList()
{
	m_pwndTransfer->OnDisableList();
}

void CTransferDlg::UpdateListCount(EWnd2 listindex, int iCount)
{
	m_pwndTransfer->UpdateListCount((CTransferWnd::EWnd2)listindex, iCount);
}

int CTransferDlg::AddCategory(const CString &newtitle, const CString &newincoming, const CString &newcomment, const CString &newautocat, bool addTab)
{
	return m_pwndTransfer->AddCategory(newtitle, newincoming, newcomment, newautocat, addTab);
}


CUploadListCtrl* CTransferDlg::GetUploadList()
{
	return &m_pwndTransfer->uploadlistctrl;
}

CDownloadListCtrl* CTransferDlg::GetDownloadList()
{
	return &m_pwndTransfer->downloadlistctrl;
}

CQueueListCtrl*	CTransferDlg::GetQueueList()
{
	return &m_pwndTransfer->queuelistctrl;
}

CClientListCtrl* CTransferDlg::GetClientList()
{
	return &m_pwndTransfer->clientlistctrl;
}

CDownloadClientsCtrl* CTransferDlg::GetDownloadClientsList()
{
	return &m_pwndTransfer->downloadclientsctrl;
}

CImageList* CTransferDlg::GetClientIconList() const
{
	return &m_pwndTransfer->m_ImageList;
}
//////////////////////////////////////////////////////////////////