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
#include "ToolBarCtrlX.h"

class CDropDownButton : public CToolBarCtrlX
{
	DECLARE_DYNAMIC(CDropDownButton)
public:
	CDropDownButton();
	virtual	~CDropDownButton() = default;

	BOOL Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID, bool bSingleDropDownBtn = true);
	BOOL Init(bool bSingleDropDownBtn = true, bool bWholeDropDown = false);

	void SetWindowText(LPCTSTR pszString);
	void SetIcon(LPCTSTR pszResourceID);
	void ResizeToMaxWidth();
	void RecalcLayout(bool bForce = false);

protected:
	bool m_bSingleDropDownBtn;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
};