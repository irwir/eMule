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
#include "MuleStatusBarCtrl.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include "ServerWnd.h"
#include "StatisticsDlg.h"
#include "ChatWnd.h"
#include "ServerConnect.h"
#include "Server.h"
#include "ServerList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CMuleStatusBarCtrl

IMPLEMENT_DYNAMIC(CMuleStatusBarCtrl, CStatusBarCtrl)

BEGIN_MESSAGE_MAP(CMuleStatusBarCtrl, CStatusBarCtrl)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

void CMuleStatusBarCtrl::Init()
{
	EnableToolTips();
}

void CMuleStatusBarCtrl::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int iPane = GetPaneAtPosition(point);
	switch (iPane) {
	case SBarLog:
		{
			CString sBar;
			sBar.Format(_T("eMule %s\n\n%s"), (LPCTSTR)GetResString(IDS_SV_LOG), (LPCTSTR)GetText(SBarLog));
			AfxMessageBox(sBar);
		}
		break;
	case SBarUsers:
	case SBarConnected:
		theApp.emuledlg->serverwnd->ShowNetworkInfo();
		break;
	case SBarUpDown:
		theApp.emuledlg->SetActiveDialog(theApp.emuledlg->statisticswnd);
		break;
	case SBarChatMsg:
		theApp.emuledlg->SetActiveDialog(theApp.emuledlg->chatwnd);
		break;
	case SBarUSS:
		theApp.emuledlg->ShowPreferences(IDD_PPG_TWEAKS);
	}
}

int CMuleStatusBarCtrl::GetPaneAtPosition(CPoint &point) const
{
	CRect rect;
	for (int i = GetParts(0, NULL); --i >= 0;) {
		GetRect(i, rect);
		if (rect.PtInRect(point))
			return i;
	}
	return -1;
}

CString CMuleStatusBarCtrl::GetPaneToolTipText(EStatusBarPane iPane) const
{
	CString strText;
	if (iPane == SBarConnected && theApp.serverconnect && theApp.serverconnect->IsConnected()) {
		CServer *cur_server = theApp.serverconnect->GetCurrentServer();
		if (cur_server) {
			CServer *srv = theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort());
			// Can't add more info than just the server name, unfortunately the MFC tooltip which
			// we use here does not show more than one(!) line of text.
			if (srv)
				strText.Format(_T("eD2K %s: %s  (%s %s)")
					, (LPCTSTR)GetResString(IDS_SERVER)
					, (LPCTSTR)srv->GetListName()
					, (LPCTSTR)GetFormatedUInt(srv->GetUsers())
					, (LPCTSTR)GetResString(IDS_UUSERS));
		}

	}
	return strText;
}

INT_PTR CMuleStatusBarCtrl::OnToolHitTest(CPoint point, TOOLINFO *pTI) const
{
	INT_PTR iHit = CWnd::OnToolHitTest(point, pTI);
	if (iHit == -1 && pTI != NULL && pTI->cbSize >= sizeof(AFX_OLDTOOLINFO)) {
		int iPane = GetPaneAtPosition(point);
		if (iPane >= 0) {
			const CString &strToolTipText = GetPaneToolTipText((EStatusBarPane)iPane);
			if (!strToolTipText.IsEmpty()) {
				pTI->hwnd = m_hWnd;
				pTI->uId = iPane;
				pTI->uFlags &= ~TTF_IDISHWND;
				pTI->uFlags |= TTF_NOTBUTTON | TTF_ALWAYSTIP;
				pTI->lpszText = _tcsdup(strToolTipText); // gets freed by MFC
				GetRect(iPane, &pTI->rect);
				iHit = iPane;
			}
		}
	}
	return iHit;
}