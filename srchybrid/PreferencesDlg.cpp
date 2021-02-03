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
#include "PreferencesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPreferencesDlg, CTreePropSheet)

BEGIN_MESSAGE_MAP(CPreferencesDlg, CTreePropSheet)
	ON_WM_DESTROY()
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CPreferencesDlg::CPreferencesDlg()
{
	m_psh.dwFlags &= ~PSH_HASHELP;
	m_wndGeneral.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndDisplay.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndConnection.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndServer.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndDirectories.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndFiles.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndStats.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndIRC.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndWebServer.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndTweaks.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndSecurity.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndScheduler.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndProxy.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndMessages.m_psp.dwFlags &= ~PSH_HASHELP;
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	m_wndDebug.m_psp.dwFlags &= ~PSH_HASHELP;
#endif

	CTreePropSheet::SetPageIcon(&m_wndGeneral, _T("Preferences"));
	CTreePropSheet::SetPageIcon(&m_wndDisplay, _T("DISPLAY"));
	CTreePropSheet::SetPageIcon(&m_wndConnection, _T("CONNECTION"));
	CTreePropSheet::SetPageIcon(&m_wndProxy, _T("PROXY"));
	CTreePropSheet::SetPageIcon(&m_wndServer, _T("SERVER"));
	CTreePropSheet::SetPageIcon(&m_wndDirectories, _T("FOLDERS"));
	CTreePropSheet::SetPageIcon(&m_wndFiles, _T("Transfer"));
	CTreePropSheet::SetPageIcon(&m_wndNotify, _T("NOTIFICATIONS"));
	CTreePropSheet::SetPageIcon(&m_wndStats, _T("STATISTICS"));
	CTreePropSheet::SetPageIcon(&m_wndIRC, _T("IRC"));
	CTreePropSheet::SetPageIcon(&m_wndSecurity, _T("SECURITY"));
	CTreePropSheet::SetPageIcon(&m_wndScheduler, _T("SCHEDULER"));
	CTreePropSheet::SetPageIcon(&m_wndWebServer, _T("WEB"));
	CTreePropSheet::SetPageIcon(&m_wndTweaks, _T("TWEAK"));
	CTreePropSheet::SetPageIcon(&m_wndMessages, _T("MESSAGES"));
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	CTreePropSheet::SetPageIcon(&m_wndDebug, _T("Preferences"));
#endif

	AddPage(&m_wndGeneral);
	AddPage(&m_wndDisplay);
	AddPage(&m_wndConnection);
	AddPage(&m_wndProxy);
	AddPage(&m_wndServer);
	AddPage(&m_wndDirectories);
	AddPage(&m_wndFiles);
	AddPage(&m_wndNotify);
	AddPage(&m_wndStats);
	AddPage(&m_wndIRC);
	AddPage(&m_wndMessages);
	AddPage(&m_wndSecurity);
	AddPage(&m_wndScheduler);
	AddPage(&m_wndWebServer);
	AddPage(&m_wndTweaks);
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	AddPage(&m_wndDebug);
#endif

	// The height of the option dialog is already too large for 640x480. To show as much as
	// possible we do not show a page caption (which is a decorative element only anyway).
	SetTreeViewMode(TRUE, ::GetSystemMetrics(SM_CYSCREEN) >= 600, TRUE);
	SetTreeWidth(170);

	m_pPshStartPage = NULL;
	m_bSaveIniFile = false;
}

void CPreferencesDlg::OnDestroy()
{
	CTreePropSheet::OnDestroy();
	if (m_bSaveIniFile) {
		thePrefs.Save();
		m_bSaveIniFile = false;
	}
	m_pPshStartPage = GetPage(GetActiveIndex())->m_psp.pszTemplate;
}

BOOL CPreferencesDlg::OnInitDialog()
{
	ASSERT(!m_bSaveIniFile);
	BOOL bResult = CTreePropSheet::OnInitDialog();
	InitWindowStyles(this);

	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == m_pPshStartPage) {
			SetActivePage(i);
			break;
		}

	Localize();
	return bResult;
}

void CPreferencesDlg::LocalizeItemText(int i, UINT strid)
{
	GetPageTreeControl()->SetItemText(GetPageTreeItem(i), GetResNoAmp(strid));
}

void CPreferencesDlg::Localize()
{
	SetTitle(GetResNoAmp(IDS_EM_PREFS));

	m_wndGeneral.Localize();
	m_wndDisplay.Localize();
	m_wndConnection.Localize();
	m_wndServer.Localize();
	m_wndDirectories.Localize();
	m_wndFiles.Localize();
	m_wndStats.Localize();
	m_wndNotify.Localize();
	m_wndIRC.Localize();
	m_wndSecurity.Localize();
	m_wndTweaks.Localize();
	m_wndWebServer.Localize();
	m_wndScheduler.Localize();
	m_wndProxy.Localize();
	m_wndMessages.Localize();

	if (GetPageTreeControl()) {
		static const UINT uids[15] =
		{
			IDS_PW_GENERAL, IDS_PW_DISPLAY, IDS_CONNECTION, IDS_PW_PROXY, IDS_PW_SERVER,
			IDS_PW_DIR, IDS_PW_FILES, IDS_PW_EKDEV_OPTIONS, IDS_STATSSETUPINFO, IDS_IRC,
			IDS_MESSAGESCOMMENTS, IDS_SECURITY, IDS_SCHEDULER, IDS_PW_WS, IDS_PW_TWEAK
		};

		int c;
		for (c = 0; c < _countof(uids); ++c)
			LocalizeItemText(c, uids[c]);
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
		GetPageTreeControl()->SetItemText(GetPageTreeItem(c), _T("Debug"));
#endif
	}

	UpdateCaption();
}

void CPreferencesDlg::OnHelp()
{
	int iCurSel = GetActiveIndex();
	if (iCurSel >= 0) {
		CPropertyPage *pPage = GetPage(iCurSel);
		if (pPage) {
			HELPINFO hi = {};
			hi.cbSize = (UINT)sizeof hi;
			hi.iContextType = HELPINFO_WINDOW;
			hi.iCtrlId = 0;
			hi.hItemHandle = pPage->m_hWnd;
			hi.dwContextId = 0;
			pPage->SendMessage(WM_HELP, 0, (LPARAM)&hi);
			return;
		}
	}

	theApp.ShowHelp(0, HELP_CONTENTS);
}

BOOL CPreferencesDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case ID_HELP:
		return OnHelpInfo(NULL);
	case IDOK:
	case ID_APPLY_NOW:
		m_bSaveIniFile = true;
	}
	return __super::OnCommand(wParam, lParam);
}

BOOL CPreferencesDlg::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}