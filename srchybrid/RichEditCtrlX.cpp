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
#include "resource.h"
#include "MenuCmds.h"
#include "RichEditCtrlX.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CRichEditCtrlX

BEGIN_MESSAGE_MAP(CRichEditCtrlX, CRichEditCtrl)
	ON_WM_CONTEXTMENU()
	ON_WM_KEYDOWN()
	ON_WM_GETDLGCODE()
	ON_NOTIFY_REFLECT_EX(EN_LINK, OnEnLink)
	ON_CONTROL_REFLECT(EN_CHANGE, OnEnChange)
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

CRichEditCtrlX::CRichEditCtrlX()
	: m_bDisableSelectOnFocus()
	, m_bSelfUpdate()
	, m_bForceArrowCursor()
	, m_hArrowCursor(::LoadCursor(NULL, IDC_ARROW))
	, m_cfDef()
	, m_cfKeyword()
{
	m_cfDef.cbSize = (UINT)sizeof m_cfDef;
	m_cfKeyword.cbSize = (UINT)sizeof m_cfKeyword;
}

void CRichEditCtrlX::SetDisableSelectOnFocus(bool bDisable)
{
	m_bDisableSelectOnFocus = bDisable;
}

void CRichEditCtrlX::SetSyntaxColoring(LPCTSTR *ppszKeywords, LPCTSTR pszSeparators)
{
	int i = 0;
	while (ppszKeywords[i] != NULL)
		m_astrKeywords.Add(ppszKeywords[i++]);
	m_strSeparators = pszSeparators;

	if (m_astrKeywords.IsEmpty())
		m_strSeparators.Empty();
	else {
		SetEventMask(GetEventMask() | ENM_CHANGE);
		GetDefaultCharFormat(m_cfDef);
		m_cfKeyword = m_cfDef;
		m_cfKeyword.dwMask |= CFM_COLOR;
		m_cfKeyword.dwEffects &= ~CFE_AUTOCOLOR;
		m_cfKeyword.crTextColor = RGB(0, 0, 255);

		ASSERT(GetTextMode() & TM_MULTILEVELUNDO);
	}
}

CRichEditCtrlX& CRichEditCtrlX::operator<<(LPCTSTR psz)
{
	ReplaceSel(psz);
	return *this;
}

CRichEditCtrlX& CRichEditCtrlX::operator<<(char *psz)
{
	ReplaceSel(CString(psz));
	return *this;
}

CRichEditCtrlX& CRichEditCtrlX::operator<<(UINT uVal)
{
	CString strVal;
	strVal.Format(_T("%u"), uVal);
	ReplaceSel(strVal);
	return *this;
}

CRichEditCtrlX& CRichEditCtrlX::operator<<(int iVal)
{
	CString strVal;
	strVal.Format(_T("%d"), iVal);
	ReplaceSel(strVal);
	return *this;
}

CRichEditCtrlX& CRichEditCtrlX::operator<<(double fVal)
{
	CString strVal;
	strVal.Format(_T("%.3f"), fVal);
	ReplaceSel(strVal);
	return *this;
}

UINT CRichEditCtrlX::OnGetDlgCode()
{
	if (m_bDisableSelectOnFocus) {
		// Avoid that the edit control will select the entire contents, if the
		// focus is moved via tab into the edit control
		//
		// DLGC_WANTALLKEYS is needed, if the control is within a wizard property
		// page and the user presses the Enter key to invoke the default button of the property sheet!
		return CRichEditCtrl::OnGetDlgCode() & ~(DLGC_HASSETSEL | DLGC_WANTALLKEYS);
	}
	// if there is an auto complete control attached to the rich edit control, we have to explicitly disable DLGC_WANTTAB
	return CRichEditCtrl::OnGetDlgCode() & ~DLGC_WANTTAB;
}

BOOL CRichEditCtrlX::OnEnLink(LPNMHDR pNMHDR, LRESULT *pResult)
{
	ENLINK *pEnLink = reinterpret_cast<ENLINK*>(pNMHDR);
	if (pEnLink && pEnLink->msg == WM_LBUTTONDOWN) {
		CString strUrl;
		GetTextRange(pEnLink->chrg.cpMin, pEnLink->chrg.cpMax, strUrl);
		BrowserOpen(strUrl, NULL);
		return (BOOL)(*pResult = 1); // do not pass this message to any parent
	}
	return (BOOL)(*pResult = 0);
}

void CRichEditCtrlX::OnContextMenu(CWnd*, CPoint point)
{
	if (!PointInClient(*this, point)) {
		Default();
		return;
	}

	long iSelStart, iSelEnd;
	GetSel(iSelStart, iSelEnd);
	int iTextLen = GetWindowTextLength();

	// Context menu of standard edit control
	//
	// Undo
	// ----
	// Cut
	// Copy
	// Paste
	// Delete
	// ------
	// Select All

	bool bReadOnly = (GetStyle() & ES_READONLY) != 0;

	CMenu menu;
	menu.CreatePopupMenu();
	if (!bReadOnly) {
		menu.AppendMenu(MF_STRING, MP_UNDO, GetResString(IDS_UNDO));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, MP_CUT, GetResString(IDS_CUT));
	}
	menu.AppendMenu(MF_STRING, MP_COPYSELECTED, GetResString(IDS_COPY));
	if (!bReadOnly) {
		menu.AppendMenu(MF_STRING, MP_PASTE, GetResString(IDS_PASTE));
		menu.AppendMenu(MF_STRING, MP_REMOVESELECTED, GetResString(IDS_DELETESELECTED));
	}
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, MP_SELECTALL, GetResString(IDS_SELECTALL));

	menu.EnableMenuItem(MP_UNDO, CanUndo() ? MF_ENABLED : MF_GRAYED);
	menu.EnableMenuItem(MP_CUT, iSelEnd > iSelStart ? MF_ENABLED : MF_GRAYED);
	menu.EnableMenuItem(MP_COPYSELECTED, iSelEnd > iSelStart ? MF_ENABLED : MF_GRAYED);
	menu.EnableMenuItem(MP_PASTE, CanPaste() ? MF_ENABLED : MF_GRAYED);
	menu.EnableMenuItem(MP_REMOVESELECTED, iSelEnd > iSelStart ? MF_ENABLED : MF_GRAYED);
	menu.EnableMenuItem(MP_SELECTALL, iTextLen > 0 ? MF_ENABLED : MF_GRAYED);

	if (point.x == -1 && point.y == -1) {
		point.SetPoint(16, 32);
		ClientToScreen(&point);
	}
	// Cheap workaround for the "Text cursor is showing while context menu is open" glitch.
	// It could be solved properly with the RE's COM interface, but because the according messages
	// are not routed with a unique control ID, it's not really usable (e.g. if there are more
	// RE controls in one window). Would need to envelope each RE window to get a unique ID.
	m_bForceArrowCursor = true;
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	m_bForceArrowCursor = false;
}

BOOL CRichEditCtrlX::OnCommand(WPARAM wParam, LPARAM)
{
	switch (wParam) {
	case MP_UNDO:
		Undo();
		break;
	case MP_CUT:
		Cut();
		break;
	case MP_COPYSELECTED:
		Copy();
		break;
	case MP_PASTE:
		Paste();
		break;
	case MP_REMOVESELECTED:
		Clear();
		break;
	case MP_SELECTALL:
		SetSel(0, -1);
	}
	return TRUE;
}

void CRichEditCtrlX::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (GetKeyState(VK_CONTROL) < 0)
		if (nChar == 'A')
			//////////////////////////////////////////////////////////////////
			// Ctrl+A: Select all
			SetSel(0, -1);
		else if (nChar == 'C')
			//////////////////////////////////////////////////////////////////
			// Ctrl+C: Copy selected contents to clipboard
			Copy();

	CRichEditCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CRichEditCtrlX::OnEnChange()
{
	if (!m_bSelfUpdate && !m_astrKeywords.IsEmpty())
		UpdateSyntaxColoring();
}

void CRichEditCtrlX::UpdateSyntaxColoring()
{
	CString strText;
	GetWindowText(strText);
	if (strText.IsEmpty())
		return;

	m_bSelfUpdate = true;

	long lCurSelStart, lCurSelEnd;
	GetSel(lCurSelStart, lCurSelEnd);
	SetSel(0, -1);
	SetSelectionCharFormat(m_cfDef);
	SetSel(lCurSelStart, lCurSelEnd);

	LPTSTR pszStart = const_cast<LPTSTR>((LPCTSTR)strText);
	LPCTSTR psz = pszStart;
	while (*psz != _T('\0')) {
		if (*psz == _T('\"')) {
			LPCTSTR pszEnd = _tcschr(psz + 1, _T('\"'));
			if (!pszEnd)
				break;
			psz = pszEnd + 1;
		} else {
			bool bFoundKeyword = false;
			for (int k = 0; k < m_astrKeywords.GetCount(); ++k) {
				const CString &rstrKeyword = m_astrKeywords[k];
				int iKwLen = rstrKeyword.GetLength();
				if (_tcsncmp(psz, rstrKeyword, iKwLen) == 0 && (psz[iKwLen] == _T('\0') || _tcschr(m_strSeparators, psz[iKwLen]) != NULL)) {
					long iStart = (long)(psz - pszStart);
					long iEnd = iStart + iKwLen;
					GetSel(lCurSelStart, lCurSelEnd);
					SetSel(iStart, iEnd);
					SetSelectionCharFormat(m_cfKeyword);
					SetSel(lCurSelStart, lCurSelEnd);
					psz += iKwLen;
					bFoundKeyword = true;
					break;
				}
			}

			if (!bFoundKeyword)
				++psz;
		}
	}

	UpdateWindow();

	m_bSelfUpdate = false;
}

BOOL CRichEditCtrlX::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	// Cheap workaround for the "Text cursor is showing while context menu is open" glitch.
	// It could be solved properly with the RE's COM interface, but because the according messages
	// are not routed with a unique control ID, it's not really usable (e.g. if there are more
	// RE controls in one window). Would need to envelope each RE window to get a unique ID.
	if (m_bForceArrowCursor && m_hArrowCursor) {
		::SetCursor(m_hArrowCursor);
		return TRUE;
	}
	return CRichEditCtrl::OnSetCursor(pWnd, nHitTest, message);
}

DWORD CALLBACK CRichEditCtrlX::StreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	CFile *pFile = reinterpret_cast<CFile*>(dwCookie);
	*pcb = pFile->Read(pbBuff, cb);
	return 0;
}

void CRichEditCtrlX::SetRTFText(const CStringA &rstrTextA)
{
	CMemFile memFile((BYTE*)(LPCSTR)rstrTextA, rstrTextA.GetLength());
	EDITSTREAM es = {};
	es.pfnCallback = StreamInCallback;
	es.dwCookie = (DWORD_PTR)&memFile;
	StreamIn(SF_RTF, es);
}