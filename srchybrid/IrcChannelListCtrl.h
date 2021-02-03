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

struct ChannelName;

class CIrcChannelListCtrl : public CMuleListCtrl
{
	DECLARE_DYNAMIC(CIrcChannelListCtrl)
	friend class CIrcWnd;
public:
	CIrcChannelListCtrl();
	virtual	~CIrcChannelListCtrl();

	void ResetServerChannelList(bool bShutDown = false);
	bool AddChannelToList(const CString &sName, const CString &sUsers, const CString &sDesc);
	void JoinChannels();
	void Localize();
	void Init();

protected:
	CTypedPtrList<CPtrList, ChannelName*> m_lstChannelNames;
	CIrcWnd *m_pParent;

	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	CString GetItemDisplayText(const ChannelName *pChannel, int iSubItem) const;

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClk(LPNMHDR pNMHDR, LRESULT *pResult);
};