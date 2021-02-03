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
#include "ResizableLib\ResizableDialog.h"
#include "ChatSelector.h"
#include "FriendListCtrl.h"
#include "SplitterControl.h"
#include "IconStatic.h"
#include "ToolBarCtrlX.h"

class CSmileySelector;

class CChatWnd : public CResizableDialog
{
	DECLARE_DYNAMIC(CChatWnd)
	friend class CChatSelector;

	enum
	{
		IDD = IDD_CHAT
	};

public:
	explicit CChatWnd(CWnd *pParent = NULL);   // standard constructor
	virtual	~CChatWnd();

	void StartSession(CUpDownClient *client);
	void Localize();
	void UpdateFriendlistCount(INT_PTR count);
	void UpdateSelectedFriendMsgDetails();
	void ScrollHistory(bool down);
	void EnableClose()						{ GetDlgItem(IDC_CCLOSE)->EnableWindow(chatselector.GetCurrentChatItem() != NULL); }
	void EnableSmileys(bool bEnable)		{ chatselector.EnableSmileys(bEnable); }

	CFriendListCtrl m_FriendListCtrl;
	CChatSelector chatselector;

protected:
	HICON icon_friend;
	HICON icon_msg;
	CSplitterControl m_wndSplitterHorz;
	CIconStatic m_cUserInfo;
	CToolBarCtrlX m_wndFormat;
	CEdit m_wndMessage;
	CButton m_wndSend;
	CButton m_wndClose;
	CSmileySelector *m_pwndSmileySel;
	void OnChatTextChange();

	void SetAllIcons();
	void DoResize(int iDelta);
	void ShowFriendMsgDetails(CFriend *pFriend);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual LRESULT DefWindowProc(UINT uMessage, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
	afx_msg LRESULT OnCloseTab(WPARAM wParam, LPARAM);
	afx_msg void OnBnClickedClose();
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedSmiley();
	afx_msg void OnLvnItemActivateFriendList(LPNMHDR, LRESULT*);
	afx_msg void OnNmClickFriendList(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnStnDblClickFriendIcon();
	afx_msg void OnSysColorChange();
};