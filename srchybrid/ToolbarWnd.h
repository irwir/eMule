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

#pragma once

class CToolBarCtrlX;
class CDownloadListCtrl;

class CToolbarWnd : public CDialogBar
{
	DECLARE_DYNAMIC(CToolbarWnd);

	enum
	{
		IDD = IDD_DOWNLOAD_TOOLBARS
	};

public:
	CToolbarWnd();
	virtual	~CToolbarWnd();

	void Localize();
	void OnAvailableCommandsChanged(CList<int> *liCommands);
	void SetCommandTargetWnd(CDownloadListCtrl *pWnd)		{ m_pCommandTargetWnd = pWnd; }

	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);
	virtual void OnUpdateCmdUI(CFrameWnd *pTarget, BOOL bDisableIfNoHndler);
	virtual BOOL PreTranslateMessage(MSG *pMsg);

protected:
	HCURSOR m_hcurMove;
	CSize m_szMRU;
	CSize m_szFloat;
	CToolBarCtrlX *m_btnBar;
	CDownloadListCtrl *m_pCommandTargetWnd;

//	void SetAllIcons();
	void FillToolbar();
	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM);
	virtual void DelayShow(BOOL bShow);

	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnInitDialog(WPARAM, LPARAM);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnSysColorChange();
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnBtnDropDown(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
};