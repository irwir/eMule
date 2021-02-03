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
#include "TrayDialog.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "MenuCmds.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	IDT_SINGLE_CLICK	100

//#define	NOTIFYICONDATA_V1_TIP_SIZE	64
#define	NOTIFYICONDATA_TIP_SIZE		128 //for Windows 2K and above

/////////////////////////////////////////////////////////////////////////////
// CTrayDialog dialog

const UINT WM_TASKBARCREATED = RegisterWindowMessage(_T("TaskbarCreated"));

BEGIN_MESSAGE_MAP(CTrayDialog, CTrayDialogBase)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SYSCOMMAND()
	ON_MESSAGE(UM_TRAY_ICON_NOTIFY_MESSAGE, OnTrayNotify)
	ON_REGISTERED_MESSAGE(WM_TASKBARCREATED, OnTaskBarCreated)
	ON_WM_TIMER()
END_MESSAGE_MAP()

CTrayDialog::CTrayDialog(UINT uIDD, CWnd *pParent /*=NULL*/)
	: CTrayDialogBase(uIDD, pParent)
	, m_pbMinimizeToTray()
	, m_hPrevIconDelete()
	, m_uSingleClickTimer()
	, m_bCurIconDelete()
	, m_bLButtonDblClk()
	, m_bLButtonDown()
	, m_bTrayIconVisible()
	//, m_nDefaultMenuItem()
{
	m_nidIconData.cbSize = NOTIFYICONDATA_V2_SIZE; //for Windows 2K and above
	m_nidIconData.hWnd = 0;
	m_nidIconData.uID = 1;
	m_nidIconData.uCallbackMessage = UM_TRAY_ICON_NOTIFY_MESSAGE;
	m_nidIconData.hIcon = 0;
	m_nidIconData.szTip[0] = _T('\0');
	m_nidIconData.uFlags = NIF_MESSAGE;
}

int CTrayDialog::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CTrayDialogBase::OnCreate(lpCreateStruct) == -1)
		return -1;

	ASSERT(WM_TASKBARCREATED);
	m_nidIconData.hWnd = m_hWnd;
	m_nidIconData.uID = 1;
	return 0;
}

void CTrayDialog::OnDestroy()
{
	KillSingleClickTimer();
	CTrayDialogBase::OnDestroy();

	// shouldn't that be done before passing the message to DefWinProc?
	if (m_nidIconData.hWnd && m_nidIconData.uID > 0 && TrayIsVisible())
		VERIFY(Shell_NotifyIcon(NIM_DELETE, &m_nidIconData));
}

bool CTrayDialog::TrayIsVisible()
{
	return m_bTrayIconVisible;
}

void CTrayDialog::TraySetIcon(HICON hIcon, bool bDelete)
{
	ASSERT(hIcon);
	if (hIcon) {
		//ASSERT(m_hPrevIconDelete == NULL);
		if (m_bCurIconDelete) {
			ASSERT(m_nidIconData.hIcon != NULL && (m_nidIconData.uFlags & NIF_ICON));
			m_hPrevIconDelete = m_nidIconData.hIcon;
		}
		m_bCurIconDelete = bDelete;
		m_nidIconData.hIcon = hIcon;
		m_nidIconData.uFlags |= NIF_ICON;
	}
}

void CTrayDialog::TraySetIcon(UINT nResourceID)
{
	TraySetIcon(AfxGetApp()->LoadIcon(nResourceID));
}

void CTrayDialog::TraySetIcon(LPCTSTR lpszResourceName)
{
	TraySetIcon(AfxGetApp()->LoadIcon(lpszResourceName));
}

void CTrayDialog::TraySetToolTip(LPCTSTR lpszToolTip)
{
	ASSERT(*lpszToolTip != '\0' && _tcslen(lpszToolTip) < NOTIFYICONDATA_TIP_SIZE);
	_tcsncpy(m_nidIconData.szTip, lpszToolTip, NOTIFYICONDATA_TIP_SIZE);
	m_nidIconData.szTip[NOTIFYICONDATA_TIP_SIZE - 1] = _T('\0');
	m_nidIconData.uFlags |= NIF_TIP;

	Shell_NotifyIcon(NIM_MODIFY, &m_nidIconData);
}

bool CTrayDialog::TrayShow()
{
	m_bTrayIconVisible = Shell_NotifyIcon(NIM_ADD, &m_nidIconData) != 0;
	return m_bTrayIconVisible;
}

bool CTrayDialog::TrayHide()
{
	bool bSuccess = Shell_NotifyIcon(NIM_DELETE, &m_nidIconData);
	if (bSuccess)
		m_bTrayIconVisible = false;
	return bSuccess;
}

BOOL CTrayDialog::TrayUpdate()
{
	BOOL bSuccess = FALSE;
	if (m_bTrayIconVisible) {
		bSuccess = Shell_NotifyIcon(NIM_MODIFY, &m_nidIconData);
		if (!bSuccess) {
			//ASSERT(0);
			return FALSE; // don't delete 'm_hPrevIconDelete' because it's still attached to the tray
		}
	}

	if (m_hPrevIconDelete != NULL) {
		VERIFY(::DestroyIcon(m_hPrevIconDelete));
		m_hPrevIconDelete = NULL;
	}

	return bSuccess;
}

BOOL CTrayDialog::TraySetMenu(UINT nResourceID)
{
	BOOL bSuccess = m_mnuTrayMenu.LoadMenu(nResourceID);
	ASSERT(bSuccess);
	return bSuccess;
}

BOOL CTrayDialog::TraySetMenu(LPCTSTR lpszMenuName)
{
	BOOL bSuccess = m_mnuTrayMenu.LoadMenu(lpszMenuName);
	ASSERT(bSuccess);
	return bSuccess;
}

BOOL CTrayDialog::TraySetMenu(HMENU hMenu)
{
	m_mnuTrayMenu.Attach(hMenu);
	return TRUE;
}

LRESULT CTrayDialog::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam != 1u) //check ID
		return 0;

	CPoint pt;
	switch (lParam) {
	case WM_MOUSEMOVE:
		::GetCursorPos(&pt);
		ClientToScreen(&pt);
		OnTrayMouseMove(pt);
		break;
	case WM_LBUTTONDOWN:
		::GetCursorPos(&pt);
		ClientToScreen(&pt);
		OnTrayLButtonDown(pt);
		break;
	case WM_LBUTTONUP:
		// Handle the WM_LBUTTONUP only if we know that there was also an according
		// WM_LBUTTONDOWN or WM_LBUTTONDBLCLK on our tray bar icon. If we would handle
		// WM_LBUTTONUP without checking this, we may get a single WM_LBUTTONUP message
		// whereby the according WM_LBUTTONDOWN message was meant for some other tray bar
		// icon.
		if (m_bLButtonDblClk || (m_bLButtonDown && !thePrefs.m_bEnableMiniMule)) { //use single click to restore from tray
			KillSingleClickTimer();
			RestoreWindow();
			if (!thePrefs.m_bEnableMiniMule) //if restoring on single click
				m_bLButtonDown = false; //reset single click too
			m_bLButtonDblClk = false;
		} else if (m_bLButtonDown) {
			if (!m_uSingleClickTimer && !IsWindowVisible())
				m_uSingleClickTimer = SetTimer(IDT_SINGLE_CLICK, 300, NULL);
			m_bLButtonDown = false;
		}
		break;
	case WM_LBUTTONDBLCLK:
		KillSingleClickTimer();
		::GetCursorPos(&pt);
		ClientToScreen(&pt);
		OnTrayLButtonDblClk(pt);
		break;
	case WM_RBUTTONUP:
	case WM_CONTEXTMENU:
		KillSingleClickTimer();
		::GetCursorPos(&pt);
		//ClientToScreen(&pt);
		OnTrayRButtonUp(pt);
		break;
	case WM_RBUTTONDBLCLK:
		KillSingleClickTimer();
		::GetCursorPos(&pt);
		ClientToScreen(&pt);
		OnTrayRButtonDblClk(pt);
	}
	return 1;
}

void CTrayDialog::KillSingleClickTimer()
{
	if (m_uSingleClickTimer) {
		VERIFY(KillTimer(m_uSingleClickTimer));
		m_uSingleClickTimer = 0;
	}
}

void CTrayDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_uSingleClickTimer) {
		TRACE("%s: nIDEvent=%u\n", __FUNCTION__, nIDEvent);
		// Kill that timer before calling 'OnTrayLButtonUp' which may create the MiniMule window asynchronously!
		KillSingleClickTimer();
		OnTrayLButtonUp(CPoint());
	}
	CDialogMinTrayBtn<CResizableDialog>::OnTimer(nIDEvent);
}

void CTrayDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (m_pbMinimizeToTray != NULL && *m_pbMinimizeToTray) {
		if ((nID & 0xFFF0) == SC_MINIMIZE) {
			if (TrayShow())
				ShowWindow(SW_HIDE);
		} else
			CTrayDialogBase::OnSysCommand(nID, lParam);
	} else if ((nID & 0xFFF0) == MP_MINIMIZETOTRAY) {
		if (TrayShow())
			ShowWindow(SW_HIDE);
	} else
		CTrayDialogBase::OnSysCommand(nID, lParam);
}

void CTrayDialog::TraySetMinimizeToTray(bool *pbMinimizeToTray)
{
	m_pbMinimizeToTray = pbMinimizeToTray;
}

void CTrayDialog::TrayMinimizeToTrayChange()
{
	if (m_pbMinimizeToTray)
		if (*m_pbMinimizeToTray || thePrefs.IsRunningAeroGlassTheme()) //there is no easy way to draw in caption
			MinTrayBtnHide();
		else
			MinTrayBtnShow();
}

void CTrayDialog::OnTrayRButtonUp(CPoint)
{
}

void CTrayDialog::OnTrayLButtonDown(CPoint)
{
	m_bLButtonDown = true;
}

void CTrayDialog::OnTrayLButtonUp(CPoint)
{
}

void CTrayDialog::OnTrayLButtonDblClk(CPoint)
{
	m_bLButtonDblClk = true;
}

void CTrayDialog::OnTrayRButtonDblClk(CPoint)
{
}

void CTrayDialog::OnTrayMouseMove(CPoint)
{
}

LRESULT CTrayDialog::OnTaskBarCreated(WPARAM, LPARAM)
{
	Shell_NotifyIcon(NIM_DELETE, &m_nidIconData); //fix for DPI change as it keeps icon
	if (m_bTrayIconVisible)
		TrayShow();
	return 0;
}

void CTrayDialog::RestoreWindow()
{
	ShowWindow(SW_SHOW);
	SetForegroundWindow();
}