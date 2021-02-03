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
#include "EditX.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CEditX, CEdit)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SETTINGCHANGE()
	ON_WM_KEYDOWN()
	ON_WM_CREATE()
END_MESSAGE_MAP()

CEditX::CEditX()
	: m_dwLastDblClick()
	, m_dwThirdClickTime()
{
}

void CEditX::PreSubclassWindow()
{
	UpdateMetrics();
	CEdit::PreSubclassWindow();
}

int CEditX::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CEdit::OnCreate(lpCreateStruct) == -1)
		return -1;
	UpdateMetrics();
	return 0;
}

void CEditX::UpdateMetrics()
{
	m_dwThirdClickTime = GetDoubleClickTime() / 2;
	m_dwThirdClickTime += (m_dwThirdClickTime * 10) / 100;
}

void CEditX::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	UpdateMetrics();
	CEdit::OnSettingChange(uFlags, lpszSection);
}

void CEditX::OnLButtonDown(UINT nFlags, CPoint point)
{
	// check for triple click: if we already had a double click, check if the current click is inside the threshold.
	if ((GetCurrentMessage()->time - m_dwLastDblClick) <= m_dwThirdClickTime) {
		SetSel(0, -1);
		// don't reset 'm_dwLastDblClick', if there is another click within the threshold time, the selection
		// would be vanish again.
	} else {
		CEdit::OnLButtonDown(nFlags, point);
		m_dwLastDblClick = 0;
	}
}

void CEditX::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CEdit::OnLButtonDblClk(nFlags, point);
	m_dwLastDblClick = GetCurrentMessage()->time;
}