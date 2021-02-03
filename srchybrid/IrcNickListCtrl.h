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
#include "MuleListCtrl.h"

struct Channel;

struct Nick
{
	CString m_sNick;
	CString m_sSymbols; //mode symbols to display in fron of the nick (user level)
	int m_iLevel;
};

class CIrcNickListCtrl : public CMuleListCtrl
{
	DECLARE_DYNAMIC(CIrcNickListCtrl)
	friend class CIrcWnd;

public:
	CIrcNickListCtrl();

	void Init();
	Nick* FindNickByName(const CString &sChannel, const CString &sName);
	Nick* FindNickByName(const Channel *pChannel, const CString &sName);
	Nick* NewNick(const CString &sChannel, const CString &sNick);
	void RefreshNickList(const Channel *pChannel);
	void RefreshNickList(const CString &sChannel);
	bool RemoveNick(const CString &sChannel, const CString &sNick);
	void DeleteAllNick(Channel *pChannel);
	void DeleteNickInAll(const CString &sNick, const CString &sMessage);
	bool ChangeNick(const CString &sChannel, const CString &sOldNick, const CString &sNewNick);
	bool ChangeNickMode(const CString &sChannel, const CString &sNick, const TCHAR cDir, const TCHAR &cMode);
	bool ChangeAllNick(const CString &sOldNick, const CString &sNewNick);
	void OpenPrivateChannel(const Nick *pNick);
	void UpdateNickCount();
	void Localize();
	CString m_sUserModeSettings;
	CString m_sUserModeSymbols;

protected:
	CIrcWnd *m_pParent;

	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClk(LPNMHDR pNMHDR, LRESULT *pResult);
};