
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
#pragma once


class CSmileySelector : public CWnd
{
	DECLARE_DYNAMIC(CSmileySelector)

public:
	CSmileySelector();
	virtual	~CSmileySelector() = default;

	BOOL Create(CWnd *pWndParent, LPCRECT pRect, CEdit *pwndEdit);

protected:
	int m_iBitmaps;
	CEdit *m_pwndEdit;
	CToolBarCtrl m_tb;

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	afx_msg void OnTbnSmileyGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnDestroy();
};
