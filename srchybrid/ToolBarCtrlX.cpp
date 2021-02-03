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
#include "ToolBarCtrlX.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CToolBarCtrlX, CToolBarCtrl)

BEGIN_MESSAGE_MAP(CToolBarCtrlX, CToolBarCtrl)
END_MESSAGE_MAP()

void CToolBarCtrlX::DeleteAllButtons()
{
	for (int i = GetButtonCount(); --i >= 0;)
		DeleteButton(i);
}

DWORD CToolBarCtrlX::GetBtnStyle(int id)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_STYLE;
	(void)GetButtonInfo(id, &tbbi);
	return tbbi.fsStyle;
}

DWORD CToolBarCtrlX::AddBtnStyle(int id, DWORD dwStyle)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_STYLE;
	(void)GetButtonInfo(id, &tbbi);
	DWORD dwOldStyle = tbbi.fsStyle;
	tbbi.fsStyle |= dwStyle;
	SetButtonInfo(id, &tbbi);
	return dwOldStyle;
}

DWORD CToolBarCtrlX::RemoveBtnStyle(int id, DWORD dwStyle)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_STYLE;
	(void)GetButtonInfo(id, &tbbi);
	DWORD dwOldStyle = tbbi.fsStyle;
	tbbi.fsStyle &= ~dwStyle;
	SetButtonInfo(id, &tbbi);
	return dwOldStyle;
}

int CToolBarCtrlX::GetBtnWidth(int nID)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_SIZE;
	(void)GetButtonInfo(nID, &tbbi);
	return tbbi.cx;
}

void CToolBarCtrlX::SetBtnWidth(int nID, int iWidth)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_SIZE;
	tbbi.cx = (WORD)iWidth;
	SetButtonInfo(nID, &tbbi);
}

CString CToolBarCtrlX::GetBtnText(int nID)
{
	TCHAR szString[512];
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_TEXT;
	tbbi.pszText = szString;
	tbbi.cchText = _countof(szString);
	GetButtonInfo(nID, &tbbi);
	szString[_countof(szString) - 1] = _T('\0');
	return szString;
}

void CToolBarCtrlX::SetBtnText(int nID, LPCTSTR pszString)
{
	TBBUTTONINFO tbbi = {};
	tbbi.cbSize = (UINT)sizeof tbbi;
	tbbi.dwMask = TBIF_TEXT;
	tbbi.pszText = const_cast<LPTSTR>(pszString);
	SetButtonInfo(nID, &tbbi);
}

CSize CToolBarCtrlX::GetPadding()
{
	LRESULT dwPadding = SendMessage(TB_GETPADDING);
	return CSize(LOWORD(dwPadding), HIWORD(dwPadding));
}

void CToolBarCtrlX::SetPadding(CSize sizPadding)
{
	SendMessage(TB_SETPADDING, 0, MAKELPARAM(sizPadding.cx, sizPadding.cy));
}


void CToolBarCtrlX::AdjustFont(int iMaxPointSize, CSize sizButton)
{
	// The toolbar control uses the font which is specified in the current system
	// metrics. It does not use the font which is used by the parent. So, if user
	// switched to "Large Font" mode in Windows System applet, we have to do some
	// adjustments because our toolbar is of fixed size and designed for 8 pt "MS Shell Dlg".
	//
	// This function is only needed when the toolbar control is dynamically created.
	// If it's created via a dialog resource the font property is handled as
	// expected even when the font is changed in system applet during runtime.
	//
	// -> Avoid to use this function, it most likely creates glitches on some systems.

	// Toolbar control is very sensitive to font changes, adjust the font
	// only if really needed.
	CFont *pFont = GetFont();
	if (pFont) {
		LOGFONT lf;
		if (pFont->GetLogFont(&lf) > 0) {
			HDC hDC = ::GetDC(HWND_DESKTOP);
			int iPointSize = -::MulDiv(lf.lfHeight, 72, ::GetDeviceCaps(hDC, LOGPIXELSY));
			::ReleaseDC(NULL, hDC);
			if (iPointSize > iMaxPointSize) {
				CWnd *pwndParent = GetParent();
				ASSERT(pwndParent != NULL);
				if (pwndParent) {
					CFont *pFontDlg = pwndParent->GetFont();
					ASSERT(pFontDlg != NULL);
					if (pFontDlg) {
						SetFont(pFontDlg);

						// Toolbar control likes to resize buttons and stuff
						// when TBSTYLE_EX_DRAWDDARROWS is applied.
						if ((GetExtendedStyle() & TBSTYLE_EX_DRAWDDARROWS) != 0) {
							SetPadding(CSize());
							SetButtonSize(sizButton);
						}
					}
				}
			}
		}
	}
}

void CToolBarCtrlX::RecalcLayout()
{
	// Force a recalc of the toolbar's layout to work around a comctl bug
	int iTextRows = GetMaxTextRows();
	SetRedraw(FALSE);
	SetMaxTextRows(iTextRows + 1);
	SetMaxTextRows(iTextRows);
	SetRedraw(TRUE);
}

int CToolBarCtrlX::AddString(const CString &strToAdd)
{
	int nLen = strToAdd.GetLength();
	TCHAR *apChar = new TCHAR[nLen + 2];
	_tcsncpy(apChar, strToAdd, nLen);
	apChar[nLen] = _T('\0');
	apChar[nLen + 1] = _T('\0');
	nLen = AddStrings(apChar);
	delete[] apChar;
	return nLen;
}