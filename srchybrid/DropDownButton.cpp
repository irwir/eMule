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
#include "DropDownButton.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CDropDownButton, CToolBarCtrlX)

BEGIN_MESSAGE_MAP(CDropDownButton, CToolBarCtrlX)
	ON_WM_SIZE()
	ON_WM_SETTINGCHANGE()
END_MESSAGE_MAP()

CDropDownButton::CDropDownButton()
{
	m_bSingleDropDownBtn = true;
}

BOOL CDropDownButton::Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID, bool bSingleDropDownBtn)
{
	m_bSingleDropDownBtn = bSingleDropDownBtn;
	dwStyle |= CCS_NOMOVEY
			| CCS_NOPARENTALIGN
			| CCS_NORESIZE		// prevent adjusting of specified width & height(!) by Create func.
			| CCS_NODIVIDER
			| TBSTYLE_LIST
			| TBSTYLE_FLAT
			| TBSTYLE_TRANSPARENT
			| 0;

	if (!CToolBarCtrlX::Create(dwStyle, rect, pParentWnd, nID))
		return FALSE;
	return Init(bSingleDropDownBtn);
}

BOOL CDropDownButton::Init(bool bSingleDropDownBtn, bool bWholeDropDown)
{
	DeleteAllButtons();
	m_bSingleDropDownBtn = bSingleDropDownBtn;

	// If a toolbar control was created indirectly via a dialog resource one can not
	// add any buttons without setting an image list before. (?)
	// So, for this to work, we have to attach an image list to the toolbar control!
	// The image list can be empty, and it does not need to be used at all, but it has
	// to be attached.
	CImageList *piml = GetImageList();
	if (piml == NULL || piml->m_hImageList == NULL) {
		CImageList iml;
		iml.Create(16, 16, ILC_COLOR, 0, 0);
		SetImageList(&iml);
		iml.Detach();
	}
	if (m_bSingleDropDownBtn) {
		TBBUTTON atb[1] = {};
		atb[0].iBitmap = -1;
		atb[0].idCommand = (int)::GetWindowLongPtr(m_hWnd, GWLP_ID);
		atb[0].fsState = TBSTATE_ENABLED;
		atb[0].fsStyle = m_bSingleDropDownBtn ? (bWholeDropDown ? BTNS_WHOLEDROPDOWN : BTNS_DROPDOWN) : BTNS_BUTTON;
		atb[0].iString = -1;
		VERIFY(AddButtons(1, atb));

		ResizeToMaxWidth();
		SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);
	}
	return TRUE;
}

void CDropDownButton::SetWindowText(LPCTSTR pszString)
{
	int id = (int)::GetWindowLongPtr(m_hWnd, GWLP_ID);
	int cx = m_bSingleDropDownBtn ? 0 : GetBtnWidth(id);

	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_TEXT;
	tbbi.pszText = const_cast<LPTSTR>(pszString);
	SetButtonInfo(id, &tbbi);

	if (cx)
		SetBtnWidth(id, cx);
}

void CDropDownButton::SetIcon(LPCTSTR pszResourceID)
{
	if (!m_bSingleDropDownBtn)
		return;

	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 1, 1);
	iml.Add(CTempIconLoader(pszResourceID));
	CImageList *pImlOld = SetImageList(&iml);
	iml.Detach();
	if (pImlOld)
		pImlOld->DeleteImageList();

	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_IMAGE;
	tbbi.iImage = 0;
	SetButtonInfo((int)::GetWindowLongPtr(m_hWnd, GWLP_ID), &tbbi);
}

void CDropDownButton::ResizeToMaxWidth()
{
	if (!m_bSingleDropDownBtn)
		return;

	CRect rcWnd;
	GetWindowRect(&rcWnd);
	if (rcWnd.Width() > 0) {
		TBBUTTONINFO tbbi = {};
		tbbi.cbSize = (UINT)sizeof tbbi;
		tbbi.dwMask = TBIF_SIZE;
		tbbi.cx = (WORD)rcWnd.Width();
		SetButtonInfo((int)::GetWindowLongPtr(m_hWnd, GWLP_ID), &tbbi);
	}
}

void CDropDownButton::OnSize(UINT nType, int cx, int cy)
{
	CToolBarCtrlX::OnSize(nType, cx, cy);

	if (cx > 0 && cy > 0)
		ResizeToMaxWidth();
}

void CDropDownButton::RecalcLayout(bool bForce)
{
	// If toolbar has at least one button with the button style BTNS_DROPDOWN, the
	// entire toolbar is resized with too large height. So, remove the BTNS_DROPDOWN
	// button style(s) and force the toolbar to resize and apply them again.
	//
	// TODO: Should be moved to CToolBarCtrlX
	int id = (int)::GetWindowLongPtr(m_hWnd, GWLP_ID);
	bool bDropDownBtn = (GetBtnStyle(id) & BTNS_DROPDOWN) != 0;
	if (bDropDownBtn || bForce) {
		if (bDropDownBtn)
			RemoveBtnStyle(id, BTNS_DROPDOWN);
		CToolBarCtrlX::RecalcLayout();
		if (bDropDownBtn)
			AddBtnStyle(id, BTNS_DROPDOWN);
	}
}

void CDropDownButton::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CToolBarCtrlX::OnSettingChange(uFlags, lpszSection);

	// The toolbar resizes itself when the system fonts were changed,
	// especially when large/small system fonts were selected. Need
	// to recalc the layout because we have a fixed control size.
	RecalcLayout();
}