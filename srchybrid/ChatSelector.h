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
#include "ClosableTabCtrl.h"
#include "Friend.h"

class CChatWnd;
class CHTRichEditCtrl;
class CUpDownClient;


///////////////////////////////////////////////////////////////////////////////
// CChatItem

class CChatItem
{
public:
	CChatItem();
	~CChatItem();

	CUpDownClient	*client;
	CHTRichEditCtrl	*log;
	CString			strMessagePending;
	CStringArray	history;
	INT_PTR			history_pos;
	bool			notify;
};


///////////////////////////////////////////////////////////////////////////////
// CChatSelector

class CChatSelector : public CClosableTabCtrl, private CFriendConnectionListener
{
	DECLARE_DYNAMIC(CChatSelector)

public:
	CChatSelector();
	virtual	~CChatSelector() = default;

	void		Init(CChatWnd *pParent);
	CChatItem*	StartSession(CUpDownClient *client, bool show = true);
	void		EndSession(CUpDownClient *client = NULL);
	int			GetTabByClient(CUpDownClient *client);
	CChatItem*	GetItemByClient(CUpDownClient *client);
	CChatItem*	GetItemByIndex(int index);
	void		ProcessMessage(CUpDownClient *sender, const CString &message);
	void		ShowCaptchaRequest(CUpDownClient *sender, HBITMAP bmpCaptcha);
	void		ShowCaptchaResult(CUpDownClient *sender, const CString &strResult);
	bool		SendMessage(const CString &rstrMessage);
	void		DeleteAllItems();
	void		ShowChat();
	void		ConnectingResult(CUpDownClient *sender, bool success);
//	void		Send();
	void		UpdateFonts(CFont *pFont);
	CChatItem*	GetCurrentChatItem();
//	BOOL		RemoveItem(int nItem)	{ return DeleteItem(nItem); }
	void		EnableSmileys(bool bEnable);
	void		ReportConnectionProgress(CUpDownClient *pClient, const CString &strProgressDesc, bool bNoTimeStamp);
	void		ClientObjectChanged(CUpDownClient *pOldClient, CUpDownClient *pNewClient);

protected:
	CChatWnd	*m_pParent;
	UINT_PTR	m_Timer;
	CImageList	m_imlChat;
	int			m_iContextIndex;
	bool		m_blinkstate;
	bool		m_lastemptyicon;

	void AddTimeStamp(CChatItem *ci);
	void SetAllIcons();
	void GetChatSize(CRect &rcChat);

	virtual int InsertItem(int nItem, TCITEM *pTabCtrlItem);
	virtual BOOL DeleteItem(int nItem);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnTcnSelChangeChatSel(LPNMHDR, LRESULT *pResult);
//	afx_msg void OnBnClickedCsend();
//	afx_msg void OnBnClickedCclose();
//	afx_msg void OnBnClickedSmiley();
	afx_msg void OnSysColorChange();
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
};