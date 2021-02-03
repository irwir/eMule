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
#include <share.h>
#include "emule.h"
#include "HTRichEditCtrl.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "MenuCmds.h"
#include "Log.h"
#include <richole.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


int CHTRichEditCtrl::sm_iSmileyClients = 0;
CComPtr<IStorage> CHTRichEditCtrl::sm_pIStorageSmileys;
CMapStringToPtr CHTRichEditCtrl::sm_aSmileyBitmaps;

IMPLEMENT_DYNAMIC(CHTRichEditCtrl, CRichEditCtrl)

BEGIN_MESSAGE_MAP(CHTRichEditCtrl, CRichEditCtrl)
	ON_WM_CONTEXTMENU()
	ON_WM_KEYDOWN()
	ON_CONTROL_REFLECT(EN_ERRSPACE, OnEnErrspace)
	ON_CONTROL_REFLECT(EN_MAXTEXT, OnEnMaxtext)
	ON_NOTIFY_REFLECT_EX(EN_LINK, OnEnLink)
	ON_WM_CREATE()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETCURSOR()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CHTRichEditCtrl::CHTRichEditCtrl()
	: m_cfDefault()
	, m_hArrowCursor(::LoadCursor(NULL, IDC_ARROW))
	, m_crBackground(CLR_DEFAULT)
	, m_crDfltBackground(CLR_DEFAULT)
	, m_crForeground(CLR_DEFAULT)
	, m_crDfltForeground(CLR_DEFAULT)
	, m_iLimitText()
	, m_bAutoScroll(true)
	, m_bEnableSmileys()
	, m_bEnErrSpace()
	, m_bForceArrowCursor()
	, m_bNoPaint()
	, m_bRestoreFormat()
	, m_bDfltForeground()
	, m_bDfltBackground()
{
	m_cfDefault.cbSize = (UINT)sizeof(CHARFORMAT2);
}

CHTRichEditCtrl::~CHTRichEditCtrl()
{
	EnableSmileys(false);
}

BOOL CHTRichEditCtrl::Create(DWORD dwStyle, const RECT &rect, CWnd *parent, UINT nID)
{
	return static_cast<CWnd*>(this)->Create(RICHEDIT_CLASS, NULL, dwStyle, rect, parent, nID);
}

void CHTRichEditCtrl::Localize()
{
}

void CHTRichEditCtrl::Init(LPCTSTR pszTitle, LPCTSTR pszSkinKey)
{
	SetProfileSkinKey(pszSkinKey);
	SetTitle(pszTitle);

	VERIFY(SendMessage(EM_SETUNDOLIMIT, 0, 0) == 0);
	int iMaxLogBuff = thePrefs.GetMaxLogBuff();
	LimitText(iMaxLogBuff ? iMaxLogBuff : 128 * 1024);
	m_iLimitText = GetLimitText();

	VERIFY(GetSelectionCharFormat(m_cfDefault));

	// prevent the RE control to change the font height within single log lines (may happen with some Unicode chars)
	LRESULT dwLangOpts = SendMessage(EM_GETLANGOPTIONS);
	SendMessage(EM_SETLANGOPTIONS, 0, (WPARAM)(dwLangOpts & ~(IMF_AUTOFONT /*| IMF_AUTOFONTSIZEADJUST*/)));
	//SendMessage(EM_SETEDITSTYLE, SES_EMULATESYSEDIT, SES_EMULATESYSEDIT);
}

void CHTRichEditCtrl::EnableSmileys(bool bEnable)
{
	if (bEnable) {
		if (!m_bEnableSmileys) {
			m_bEnableSmileys = true;
			++sm_iSmileyClients;
		}
	} else {
		if (m_bEnableSmileys) {
			m_bEnableSmileys = false;
			--sm_iSmileyClients;
			if (sm_iSmileyClients <= 0) {
				PurgeSmileyCaches();
				sm_iSmileyClients = 0;
			}
		}
	}
}

void CHTRichEditCtrl::PurgeSmileyCaches()
{
	CString strKey;
	for (POSITION pos = sm_aSmileyBitmaps.GetStartPosition(); pos != NULL;) {
		void *pValue;
		sm_aSmileyBitmaps.GetNextAssoc(pos, strKey, pValue);
#ifdef USE_METAFILE
		VERIFY(::DeleteEnhMetaFile((HENHMETAFILE)pValue));
#else
		VERIFY(::DeleteObject((HBITMAP)pValue));
#endif
	}
	sm_aSmileyBitmaps.RemoveAll();
	sm_pIStorageSmileys.Release();
}

void CHTRichEditCtrl::SetProfileSkinKey(LPCTSTR pszSkinKey)
{
	m_strSkinKey = pszSkinKey;
}

void CHTRichEditCtrl::SetTitle(LPCTSTR pszTitle)
{
	m_strTitle = pszTitle;
}

int CHTRichEditCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CRichEditCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	Init(NULL);
	return 0;
}

LRESULT CHTRichEditCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_ERASEBKGND:
	case WM_PAINT:
		if (m_bNoPaint)
			return TRUE;
	}
	return CRichEditCtrl::WindowProc(message, wParam, lParam);
}

void CHTRichEditCtrl::OnSize(UINT nType, int cx, int cy)
{
	// Use the 'ScrollInfo' only, if there is a scrollbar available, otherwise we would
	// use a scrollinfo which points to the top and we would thus stay at the top.
	bool bAtEndOfScroll;
	SCROLLINFO si;
	si.cbSize = (UINT)sizeof si;
	si.fMask = SIF_ALL;
	if ((GetStyle() & WS_VSCROLL) && GetScrollInfo(SB_VERT, &si))
		bAtEndOfScroll = (si.nPos >= (int)(si.nMax - si.nPage));
	else
		bAtEndOfScroll = true;

	CRichEditCtrl::OnSize(nType, cx, cy);

	if (bAtEndOfScroll)
		ScrollToLastLine();
}

COLORREF GetLogLineColor(UINT eMsgType)
{
	switch (eMsgType) {
	case LOG_SUCCESS:
		return thePrefs.m_crLogSuccess;
	case LOG_ERROR:
		return thePrefs.m_crLogError;
	case LOG_WARNING:
		return thePrefs.m_crLogWarning;
	default:
		ASSERT(eMsgType == LOG_INFO);
	}
	return CLR_DEFAULT;
}

void CHTRichEditCtrl::FlushBuffer()
{
	if (!m_astrBuff.IsEmpty()) { // flush buffer
		for (int i = 0; i < m_astrBuff.GetCount(); ++i) {
			const CString &rstrLine = m_astrBuff[i];
			if (!rstrLine.IsEmpty()) {
				if ((UINT)rstrLine[0] < 8)
					AddLine((LPCTSTR)rstrLine + 1, rstrLine.GetLength() - 1, false, GetLogLineColor((UINT)rstrLine[0]));
				else
					AddLine((LPCTSTR)rstrLine, rstrLine.GetLength());
			}
		}
		m_astrBuff.RemoveAll();
	}
}

void CHTRichEditCtrl::AddEntry(LPCTSTR pszMsg)
{
	CString strLine(pszMsg);
	strLine += _T('\n');
	if (m_hWnd == NULL)
		m_astrBuff.Add(strLine);
	else {
		FlushBuffer();
		AddLine(strLine, strLine.GetLength());
	}
}

void CHTRichEditCtrl::Add(LPCTSTR pszMsg, int iLen)
{
	if (m_hWnd == NULL)
		m_astrBuff.Add(CString(pszMsg));
	else {
		FlushBuffer();
		AddLine(pszMsg, iLen);
	}
}

void CHTRichEditCtrl::AddTyped(LPCTSTR pszMsg, int iLen, UINT eMsgType)
{
	if (m_hWnd == NULL) {
		CString strLine((TCHAR)(eMsgType & LOGMSGTYPEMASK));
		m_astrBuff.Add(strLine + pszMsg);
	} else {
		FlushBuffer();
		AddLine(pszMsg, iLen, false, GetLogLineColor(eMsgType & LOGMSGTYPEMASK));
	}
}

void CHTRichEditCtrl::AddLine(LPCTSTR pszMsg, int iLen, bool bLink, COLORREF cr, COLORREF bk, DWORD mask)
{
	int iMsgLen = (iLen == -1) ? (int)_tcslen(pszMsg) : iLen;
	if (iMsgLen == 0)
		return;

	// Get Edit contents dimensions and cursor position
	long lStartChar, lEndChar;
	GetSel(lStartChar, lEndChar);
	int iSize = GetWindowTextLength();

	// Get Auto-AutoScroll state depending on scrollbar position
	bool bAutoAutoScroll = m_bAutoScroll;
	// Use the 'ScrollInfo' only, if there is a scrollbar available, otherwise we would
	// use a scrollinfo which points to the top and we would thus stay at the top.
	SCROLLINFO si;
	si.cbSize = (UINT)sizeof si;
	si.fMask = SIF_ALL;
	if ((GetStyle() & WS_VSCROLL) && GetScrollInfo(SB_VERT, &si)) {
		// use some threshold to determine if at end or "very near" at end, unfortunately
		// this is needed to get around richedit specific stuff. this threshold (pixels)
		// should somewhat reflect the font size used in the control.
		bAutoAutoScroll = (si.nPos >= (int)(si.nMax - si.nPage - 20));
	}

	// Reduce flicker by ignoring WM_PAINT
	m_bNoPaint = true;
	BOOL bIsVisible = IsWindowVisible();
	if (bIsVisible)
		SetRedraw(FALSE);

	// Remember where we are
	//int iFirstLine = !bAutoAutoScroll ? GetFirstVisibleLine() : 0;
	POINT ptScrollPos;
	SendMessage(EM_GETSCROLLPOS, 0, (LPARAM)&ptScrollPos);

	// Select at the end of text and replace the selection
	SafeAddLine(iSize, pszMsg, iMsgLen, lStartChar, lEndChar, bLink, cr, bk, mask);
	SetSel(lStartChar, lEndChar); // Restore previous selection

	if (bAutoAutoScroll)
		ScrollToLastLine();
	else {
		//LineScroll(iFirstLine - GetFirstVisibleLine());
		SendMessage(EM_SETSCROLLPOS, 0, (LPARAM)&ptScrollPos);
	}

	m_bNoPaint = false;
	if (bIsVisible) {
		SetRedraw();
		Invalidate();
	}
}

void CHTRichEditCtrl::OnEnErrspace()
{
	m_bEnErrSpace = true;
}

void CHTRichEditCtrl::OnEnMaxtext()
{
	m_bEnErrSpace = true;
}

void CHTRichEditCtrl::ScrollToLastLine(bool bForceLastLineAtBottom)
{
	if (bForceLastLineAtBottom) {
		int iFirstVisible = GetFirstVisibleLine();
		if (iFirstVisible > 0)
			LineScroll(-iFirstVisible);
	}
	SendMessage(WM_VSCROLL, SB_BOTTOM);
}

void CHTRichEditCtrl::ScrollToFirstLine()
{
	SendMessage(WM_VSCROLL, SB_TOP);
}

void CHTRichEditCtrl::AddString(int nPos, LPCTSTR pszString, bool bLink, COLORREF cr, COLORREF bk, DWORD mask)
{
	bool bRestoreFormat = false;
	m_bEnErrSpace = false;
	SetSel(nPos, nPos);
	if (bLink) {
		CHARFORMAT2 cf = {};
		cf.cbSize = (UINT)sizeof cf;
		GetSelectionCharFormat(cf);
		cf.dwMask |= CFM_LINK;
		cf.dwEffects |= CFE_LINK;
		SetSelectionCharFormat(cf);
	} else if (cr != CLR_DEFAULT || bk != CLR_DEFAULT || (mask & (CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE)) != 0) {
		CHARFORMAT2 cf = {};
		cf.cbSize = (UINT)sizeof cf;
		GetSelectionCharFormat(cf);

		cf.dwMask |= CFM_COLOR;
		if (cr == CLR_DEFAULT) {
			if (m_bDfltForeground)
				cf.dwEffects |= CFE_AUTOCOLOR;
			else {
				ASSERT(m_crForeground != CLR_DEFAULT);
				cf.dwEffects &= ~CFE_AUTOCOLOR;
				cf.crTextColor = m_crForeground;
			}
		} else {
			cf.dwEffects &= ~CFE_AUTOCOLOR;
			cf.crTextColor = cr;
		}

		if (bk == CLR_DEFAULT) {
			// Background color is a little different than foreground color. Even if the
			// background color is set to a non-standard value (e.g. via skin), the
			// CFE_AUTOBACKCOLOR can be used to get this value. The usage of this flag instead
			// of the explicit color has the advantage that on a change of the skin or on a
			// change of the windows system colors, the control can display the colored
			// text in the new color scheme better (e.g. the text is at least readable and
			// not shown as black-on-black or white-on-white).
			//if (m_bDfltBackground) {
			cf.dwMask |= CFM_BACKCOLOR;
			cf.dwEffects |= CFE_AUTOBACKCOLOR;
		//}
		//else {
		//	ASSERT( m_crBackground != CLR_DEFAULT );
		//	cf.dwEffects &= ~CFE_AUTOBACKCOLOR;
		//	cf.crBackColor = m_crBackground;
		//}
		} else {
			cf.dwMask |= CFM_BACKCOLOR;
			cf.dwEffects &= ~CFE_AUTOBACKCOLOR;
			cf.crBackColor = bk;
		}

		cf.dwMask |= mask;

		if (mask & CFM_BOLD)
			cf.dwEffects |= CFE_BOLD;
		else if (cf.dwEffects & CFE_BOLD)
			cf.dwEffects ^= CFE_BOLD;

		if (mask & CFM_ITALIC)
			cf.dwEffects |= CFE_ITALIC;
		else if (cf.dwEffects & CFE_ITALIC)
			cf.dwEffects ^= CFE_ITALIC;

		if (mask & CFM_UNDERLINE)
			cf.dwEffects |= CFE_UNDERLINE;
		else if (cf.dwEffects & CFE_UNDERLINE)
			cf.dwEffects ^= CFE_UNDERLINE;

		SetSelectionCharFormat(cf);
		bRestoreFormat = true;
	} else if (m_bRestoreFormat)
		SetSelectionCharFormat(m_cfDefault);

	if (m_bEnableSmileys)
		AddSmileys(pszString);
	else
		ReplaceSel(pszString);

	m_bRestoreFormat = bRestoreFormat;
}

void CHTRichEditCtrl::SafeAddLine(int nPos, LPCTSTR pszLine, int iLen, long &lStartChar, long &lEndChar, bool bLink, COLORREF cr, COLORREF bk, DWORD mask)
{
	// EN_ERRSPACE and EN_MAXTEXT are not working for rich edit control (at least not same as for standard control),
	// need to explicitly check the log buffer limit.
	int iCurSize = nPos;
	if (iCurSize + iLen >= m_iLimitText) {
		bool bOldNoPaint = m_bNoPaint;
		m_bNoPaint = true;
		BOOL bIsVisible = IsWindowVisible();
		if (bIsVisible)
			SetRedraw(FALSE);

		while (iCurSize > 0 && iCurSize + iLen > m_iLimitText) {
			// delete 1st line
			int iLine0Len = LineLength(0) + 1; // add NL character
			SetSel(0, iLine0Len);
			ReplaceSel(_T(""));

			// update any possible available selection
			lStartChar -= iLine0Len;
			if (lStartChar < 0)
				lStartChar = 0;
			lEndChar -= iLine0Len;
			if (lEndChar < 0)
				lEndChar = 0;

			iCurSize = GetWindowTextLength();
		}

		m_bNoPaint = bOldNoPaint;
		if (bIsVisible && !m_bNoPaint) {
			SetRedraw();
			Invalidate();
		}
	}

	AddString(nPos, pszLine, bLink, cr, bk, mask);

	if (m_bEnErrSpace) {
		bool bOldNoPaint = m_bNoPaint;
		m_bNoPaint = true;
		BOOL bIsVisible = IsWindowVisible();
		if (bIsVisible)
			SetRedraw(FALSE);

		// remove the first line as long as we are capable of adding the new line
		int iSafetyCounter = 0;
		while (m_bEnErrSpace && iSafetyCounter < 10) {
			// delete the previous partially added line
			SetSel(nPos, -1);
			ReplaceSel(_T(""));

			// delete 1st line
			int iLine0Len = LineLength(0) + 1; // add NL character
			SetSel(0, iLine0Len);
			ReplaceSel(_T(""));

			// update any possible available selection
			lStartChar -= iLine0Len;
			if (lStartChar < 0)
				lStartChar = 0;
			lEndChar -= iLine0Len;
			if (lEndChar < 0)
				lEndChar = 0;

			// add the new line again
			nPos = GetWindowTextLength();
			AddString(nPos, pszLine, bLink, cr, bk, mask);

			if (m_bEnErrSpace && nPos == 0) {
				// should never happen: if we tried to add the line another time in the 1st line, there
				// will be no chance to add the line at all -> avoid endless loop!
				break;
			}
			++iSafetyCounter; // never ever create an endless loop!
		}
		m_bNoPaint = bOldNoPaint;
		if (bIsVisible && !m_bNoPaint) {
			SetRedraw();
			Invalidate();
		}
	}
}

void CHTRichEditCtrl::Reset()
{
	m_astrBuff.RemoveAll();
	SetRedraw(FALSE);
	SetWindowText(_T(""));
	SetRedraw();
	Invalidate();
}

void CHTRichEditCtrl::OnContextMenu(CWnd*, CPoint point)
{
	if (!PointInClient(*this, point)) {
		Default();
		return;
	}

	long lSelStart, lSelEnd;
	GetSel(lSelStart, lSelEnd);

	// ugly, simulate a left click to get around the text cursor problem when right clicking.
	if (point.x != -1 && point.y != -1 && lSelStart == lSelEnd) {
		ASSERT(GetStyle() & ES_NOHIDESEL); // this works only if ES_NOHIDESEL is set
		CPoint ptMouse(point);
		ScreenToClient(&ptMouse);
		SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, MAKELONG(ptMouse.x, ptMouse.y));
		SendMessage(WM_LBUTTONUP, MK_LBUTTON, MAKELONG(ptMouse.x, ptMouse.y));
	}

	int iTextLen = GetWindowTextLength();

	CTitleMenu menu;
	menu.CreatePopupMenu();
	menu.AddMenuTitle(GetResString(IDS_LOGENTRY));
	menu.AppendMenu(MF_STRING | (lSelEnd > lSelStart ? MF_ENABLED : MF_GRAYED), MP_COPYSELECTED, GetResString(IDS_COPY));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (iTextLen > 0 ? MF_ENABLED : MF_GRAYED), MP_SELECTALL, GetResString(IDS_SELECTALL));
	menu.AppendMenu(MF_STRING | (iTextLen > 0 ? MF_ENABLED : MF_GRAYED), MP_REMOVEALL, GetResString(IDS_PW_RESET));
	menu.AppendMenu(MF_STRING | (iTextLen > 0 ? MF_ENABLED : MF_GRAYED), MP_SAVELOG, GetResString(IDS_SAVELOG) + _T("..."));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING | (m_bAutoScroll ? MF_CHECKED : MF_UNCHECKED), MP_AUTOSCROLL, GetResString(IDS_AUTOSCROLL));

	if (point.x == -1 && point.y == -1) {
		point.SetPoint(16, 32);
		ClientToScreen(&point);
	}

	// Cheap workaround for the "Text cursor is showing while context menu is open" glitch. It could be solved properly
	// with the RE's COM interface, but because the according messages are not routed with a unique control ID, it's not
	// really usable (e.g. if there are more RE controls in one window). Would to envelope each RE window to get a unique ID.
	m_bForceArrowCursor = true;
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	m_bForceArrowCursor = false;

	VERIFY(menu.DestroyMenu());
}

BOOL CHTRichEditCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	switch (wParam) {
	case MP_COPYSELECTED:
		CopySelectedItems();
		break;
	case MP_SELECTALL:
		SelectAllItems();
		break;
	case MP_REMOVEALL:
		Reset();
		break;
	case MP_SAVELOG:
		SaveLog();
		break;
	case MP_AUTOSCROLL:
		m_bAutoScroll = !m_bAutoScroll;
	}
	return TRUE;
}

bool CHTRichEditCtrl::SaveLog(LPCTSTR pszDefName)
{
	bool bResult = false;
	const CString &fname(pszDefName ? CString(pszDefName) : m_strTitle);
	CFileDialog dlg(FALSE, _T("log"), (LPCTSTR)ValidFilename(fname), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Log Files (*.log)|*.log||"), this, 0);
	if (dlg.DoModal() == IDOK) {
		FILE *fp = _tfsopen(dlg.GetPathName(), _T("wb"), _SH_DENYWR);
		if (fp) {
			// write Unicode byte order mark 0xFEFF
			fputwc(0xFEFFui16, fp);

			CString strText;
			GetWindowText(strText);
			fwrite(strText, sizeof(TCHAR), strText.GetLength(), fp);
			if (ferror(fp)) {
				CString strError;
				strError.Format(_T("Failed to write log file \"%s\" - %s"), (LPCTSTR)dlg.GetPathName(), _tcserror(errno));
				AfxMessageBox(strError, MB_ICONERROR);
			} else
				bResult = true;
			fclose(fp);
		} else {
			CString strError;
			strError.Format(_T("Failed to create log file \"%s\" - %s"), (LPCTSTR)dlg.GetPathName(), _tcserror(errno));
			AfxMessageBox(strError, MB_ICONERROR);
		}
	}
	return bResult;
}

CString CHTRichEditCtrl::GetLastLogEntry()
{
	CString strLog;
	int iLastLine = GetLineCount() - 2;
	if (iLastLine >= 0) {
		GetLine(iLastLine, strLog.GetBuffer(1024), 1024);
		strLog.ReleaseBuffer();
	}
	return strLog;
}

CString CHTRichEditCtrl::GetAllLogEntries()
{
	CString strLog;
	GetWindowText(strLog);
	return strLog;
}

void CHTRichEditCtrl::SelectAllItems()
{
	SetSel(0, -1);
}

void CHTRichEditCtrl::CopySelectedItems()
{
	Copy();
}

void CHTRichEditCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (GetKeyState(VK_CONTROL) < 0) { //Ctrl is down
		//////////////////////////////////////////////////////////////////
		// Ctrl+A: Select all items
		if (nChar == 'A')
			SelectAllItems();
		else if (nChar == 'C')
		//////////////////////////////////////////////////////////////////
		// Ctrl+C: Copy listview items to clipboard
			CopySelectedItems();
	} else if (nChar == VK_ESCAPE)
		// don't minimize CHTRichEditCtrl
		return;

	CRichEditCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CHTRichEditCtrl::AppendText(const CString &sText)
{
	LPCTSTR psz = sText;
	LPCTSTR pszStart = psz;
	while (*psz != _T('\0')) {
		bool bFoundScheme = false;
		for (unsigned i = 0; i < _countof(s_apszSchemes); ++i) {
			if (_tcsncmp(psz, s_apszSchemes[i].pszScheme, s_apszSchemes[i].iLen) == 0) {
				// output everything before the URL
				if (psz - pszStart > 0) {
					CString str(pszStart, (int)(psz - pszStart));
					AddLine(str, str.GetLength());
				}

				// search next space or EOL
				int iLen = (int)_tcscspn(psz, _T(" \t\r\n"));
				if (iLen == 0) {
					AddLine(psz, -1, true);
					psz += _tcslen(psz);
				} else {
					CString str(psz, iLen);
					AddLine(str, str.GetLength(), true);
					psz += iLen;
				}
				pszStart = psz;
				bFoundScheme = true;
				break;
			}
		}
		if (!bFoundScheme)
			psz = _tcsinc(psz);
	}

	if (*pszStart != _T('\0'))
		AddLine(pszStart, -1);
}

void CHTRichEditCtrl::AppendHyperLink(const CString &sText, const CString &sTitle, const CString &sCommand, const CString &sDirectory)
{
	UNREFERENCED_PARAMETER(sText);
	UNREFERENCED_PARAMETER(sTitle);
	UNREFERENCED_PARAMETER(sDirectory);
	ASSERT(sText.IsEmpty());
	ASSERT(sTitle.IsEmpty());
	ASSERT(sDirectory.IsEmpty());
	AddLine(sCommand, sCommand.GetLength(), true);
}

void CHTRichEditCtrl::AppendColoredText(LPCTSTR pszText, COLORREF cr, COLORREF bk, DWORD mask)
{
	AddLine(pszText, -1, false, cr, bk, mask);
}

void CHTRichEditCtrl::AppendKeyWord(const CString &str, COLORREF cr)
{
	AppendColoredText(str, cr);
}

BOOL CHTRichEditCtrl::OnEnLink(LPNMHDR pNMHDR, LRESULT *pResult)
{
	ENLINK *pEnLink = reinterpret_cast<ENLINK*>(pNMHDR);
	if (pEnLink && pEnLink->msg == WM_LBUTTONDOWN) {
		CString strUrl;
		GetTextRange(pEnLink->chrg.cpMin, pEnLink->chrg.cpMax, strUrl);

		// check if that "URL" has a valid URL scheme. if it has not, pass that notification up to the
		// parent window which may interpret that "URL" in some other way.
		for (unsigned i = 0; i < _countof(s_apszSchemes); ++i)
			if (_tcsncmp(strUrl, s_apszSchemes[i].pszScheme, s_apszSchemes[i].iLen) == 0) {
				BrowserOpen(strUrl, NULL);
				return (BOOL)(*pResult = 1); // do not pass this message to any parent
			}
	}
	return (BOOL)(*pResult = 0);
}

CString CHTRichEditCtrl::GetText() const
{
	CString strText;
	GetWindowText(strText);
	return strText;
}

void CHTRichEditCtrl::SetFont(CFont *pFont, BOOL bRedraw)
{
	// Use the 'ScrollInfo' only, if there is a scrollbar available, otherwise we would
	// use a scrollinfo which points to the top and we would thus stay at the top.
	bool bAtEndOfScroll;
	SCROLLINFO si;
	si.cbSize = (UINT)sizeof si;
	si.fMask = SIF_ALL;
	if ((GetStyle() & WS_VSCROLL) && GetScrollInfo(SB_VERT, &si))
		bAtEndOfScroll = (si.nPos >= (int)(si.nMax - si.nPage));
	else
		bAtEndOfScroll = true;

	LOGFONT lf;
	pFont->GetLogFont(&lf);

	CHARFORMAT cf = {};
	cf.cbSize = (UINT)sizeof cf;

	cf.dwMask |= CFM_BOLD;
	if (lf.lfWeight == FW_BOLD)
		cf.dwEffects |= CFE_BOLD;

	cf.dwMask |= CFM_ITALIC;
	if (lf.lfItalic)
		cf.dwEffects |= CFE_ITALIC;

	cf.dwMask |= CFM_UNDERLINE;
	if (lf.lfUnderline)
		cf.dwEffects |= CFE_UNDERLINE;

	cf.dwMask |= CFM_STRIKEOUT;
	if (lf.lfStrikeOut)
		cf.dwEffects |= CFE_STRIKEOUT;

	cf.dwMask |= CFM_SIZE;
	HDC hDC = ::GetDC(HWND_DESKTOP);
	int iPointSize = -::MulDiv(lf.lfHeight, 72, ::GetDeviceCaps(hDC, LOGPIXELSY));
	cf.yHeight = iPointSize * 20;
	::ReleaseDC(NULL, hDC);

	cf.dwMask |= CFM_FACE;
	cf.bPitchAndFamily = lf.lfPitchAndFamily;
	_tcsncpy(cf.szFaceName, lf.lfFaceName, _countof(cf.szFaceName));
	cf.szFaceName[_countof(cf.szFaceName) - 1] = _T('\0');

	// although this should work correctly (according to SDK) it may give false results (e.g. the "click here..." text
	// which is shown in the server info window may not be entirely used as a hyperlink???)
	//cf.dwMask |= CFM_CHARSET;
	//cf.bCharSet = lf.lfCharSet;

	cf.yOffset = 0;
	VERIFY(SetDefaultCharFormat(cf));

	// copy everything except the color
	m_cfDefault.dwMask = (cf.dwMask & ~CFM_COLOR) | (m_cfDefault.dwMask & CFM_COLOR);
	m_cfDefault.dwEffects = cf.dwEffects;
	m_cfDefault.yHeight = cf.yHeight;
	m_cfDefault.yOffset = cf.yOffset;
	//m_cfDefault.crTextColor = cf.crTextColor;
	m_cfDefault.bCharSet = cf.bCharSet;
	m_cfDefault.bPitchAndFamily = cf.bPitchAndFamily;
	memcpy(m_cfDefault.szFaceName, cf.szFaceName, sizeof m_cfDefault.szFaceName);

	PurgeSmileyCaches();

	if (bAtEndOfScroll)
		ScrollToLastLine();

	if (bRedraw) {
		Invalidate();
		UpdateWindow();
	}
}

CFont* CHTRichEditCtrl::GetFont() const
{
	ASSERT(0);
	return NULL;
}

void CHTRichEditCtrl::OnSysColorChange()
{
	CRichEditCtrl::OnSysColorChange();
	ApplySkin();
}

void CHTRichEditCtrl::ApplySkin()
{
	if (m_strSkinKey.IsEmpty()) {
		m_bDfltForeground = m_crDfltForeground == CLR_DEFAULT;
		m_crForeground = m_bDfltForeground ? ::GetSysColor(COLOR_WINDOWTEXT) : m_crDfltForeground;

		m_bDfltBackground = m_crDfltBackground == CLR_DEFAULT;
		m_crBackground = m_bDfltBackground ? ::GetSysColor(COLOR_WINDOW) : m_crDfltBackground;

		VERIFY(GetSelectionCharFormat(m_cfDefault));
	} else {
		// Use the 'ScrollInfo' only, if there is a scrollbar available, otherwise we would
		// use a scrollinfo which points to the top and we would thus stay at the top.
		bool bAtEndOfScroll;
		SCROLLINFO si;
		si.cbSize = (UINT)sizeof si;
		si.fMask = SIF_ALL;
		if ((GetStyle() & WS_VSCROLL) && GetScrollInfo(SB_VERT, &si))
			bAtEndOfScroll = (si.nPos >= (int)(si.nMax - si.nPage));
		else
			bAtEndOfScroll = true;

		COLORREF cr;
		if (theApp.LoadSkinColor(m_strSkinKey + _T("Fg"), cr)) {
			m_bDfltForeground = false;
			m_crForeground = cr;
		} else {
			m_bDfltForeground = m_crDfltForeground == CLR_DEFAULT;
			m_crForeground = m_bDfltForeground ? ::GetSysColor(COLOR_WINDOWTEXT) : m_crDfltForeground;
		}

		bool bSetCharFormat = false;
		CHARFORMAT cf;
		GetDefaultCharFormat(cf);
		if (!m_bDfltForeground && (cf.dwEffects & CFE_AUTOCOLOR)) {
			cf.dwEffects &= ~CFE_AUTOCOLOR;
			bSetCharFormat = true;
		} else if (m_bDfltForeground && !(cf.dwEffects & CFE_AUTOCOLOR)) {
			cf.dwEffects |= CFE_AUTOCOLOR;
			bSetCharFormat = true;
		}
		if (bSetCharFormat) {
			cf.dwMask |= CFM_COLOR;
			cf.crTextColor = m_crForeground;
			VERIFY(SetDefaultCharFormat(cf));
			VERIFY(GetSelectionCharFormat(m_cfDefault));
		}

		if (theApp.LoadSkinColor(m_strSkinKey + _T("Bk"), cr)) {
			m_bDfltBackground = false;
			m_crBackground = cr;
			SetBackgroundColor(FALSE, m_crBackground);
		} else {
			m_bDfltBackground = m_crDfltBackground == CLR_DEFAULT;
			m_crBackground = m_bDfltBackground ? ::GetSysColor(COLOR_WINDOW) : m_crDfltBackground;
			SetBackgroundColor(m_bDfltBackground, m_crBackground);
		}

		if (bAtEndOfScroll)
			ScrollToLastLine();
	}
	PurgeSmileyCaches();
}

BOOL CHTRichEditCtrl::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	// Cheap workaround for the "Text cursor is showing while context menu is open" glitch. It could be solved properly
	// with the RE's COM interface, but because the according messages are not routed with a unique control ID, it's not
	// really usable (e.g. if there are more RE controls in one window). Would to envelope each RE window to get a unique ID.
	if (m_bForceArrowCursor && m_hArrowCursor) {
		::SetCursor(m_hArrowCursor);
		return TRUE;
	}
	return CRichEditCtrl::OnSetCursor(pWnd, nHitTest, message);
}


class CBitmapDataObject : public CCmdTarget
{
public:
	explicit CBitmapDataObject(HBITMAP hBitmap);
	virtual	~CBitmapDataObject() = default;

protected:
	DECLARE_INTERFACE_MAP();

	BEGIN_INTERFACE_PART(DataObject, IDataObject)
		STDMETHOD(GetData)(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
		STDMETHOD(GetDataHere)(FORMATETC *pformatetc, STGMEDIUM *pmedium);
		STDMETHOD(QueryGetData)(FORMATETC *pformatetc);
		STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
		STDMETHOD(SetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
		STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
		STDMETHOD(DAdvise)(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
		STDMETHOD(DUnadvise)(DWORD dwConnection);
		STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise);
	END_INTERFACE_PART(DataObject)

	HBITMAP m_hBitmap;
};

BEGIN_INTERFACE_MAP(CBitmapDataObject, CCmdTarget)
	INTERFACE_PART(CBitmapDataObject, IID_IDataObject, DataObject)
END_INTERFACE_MAP()

CBitmapDataObject::CBitmapDataObject(HBITMAP hBitmap)
{
	m_hBitmap = hBitmap;
}

#pragma warning(push)
#pragma warning(disable:4100) // unreferenced parameter
#pragma warning(disable:4555) // expression has no effect; expected expression with side-effect (because of the 'METHOD_PROLOGUE' macro)

STDMETHODIMP CBitmapDataObject::XDataObject::QueryInterface(REFIID riid, void **ppvObj) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return (HRESULT)pThis->ExternalQueryInterface(&riid, ppvObj);
}

STDMETHODIMP_(ULONG) CBitmapDataObject::XDataObject::AddRef() noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return pThis->ExternalAddRef();
}

STDMETHODIMP_(ULONG) CBitmapDataObject::XDataObject::Release() noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return pThis->ExternalRelease();
}

STDMETHODIMP CBitmapDataObject::XDataObject::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
#ifdef USE_METAFILE
	pmedium->tymed = TYMED_ENHMF;
	pmedium->hEnhMetaFile = (HENHMETAFILE)pThis->m_hBitmap;
#else
	pmedium->tymed = TYMED_GDI;
	pmedium->hBitmap = (HBITMAP)CopyImage(pThis->m_hBitmap, IMAGE_BITMAP, 0, 0, 0);
#endif
	pmedium->pUnkForRelease = NULL;
	return S_OK;
}

STDMETHODIMP CBitmapDataObject::XDataObject::GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::QueryGetData(FORMATETC *pformatetc) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::DUnadvise(DWORD dwConnection) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

STDMETHODIMP CBitmapDataObject::XDataObject::EnumDAdvise(IEnumSTATDATA **ppenumAdvise) noexcept
{
	METHOD_PROLOGUE(CBitmapDataObject, DataObject);
	return E_NOTIMPL;
}

#pragma warning(pop)

static const struct
{
	LPCTSTR pszSmiley;
	int iLen;
	LPCTSTR pszID;
} s_apszSmileys[] =
{
	// :) :))) ;) :D :/ :P :-x :( :'-( :-| :-* :ph34r: =) :] :[ :-O <_<
#define	S(str, id)	 { _T(str), _countof(str)-1, _T(id) }
	S(":)",         "smile"),
	S(":-)",        "smile"),

	S(":]",         "smileq"),
	S(":-]",        "smileq"),

	S(":))",		"happy"),
	S(":)))",		"happy"),
	S(":-))",		"happy"),
	S(":-)))",		"happy"),
	S("^_^",		"happy"),
	S("(^.^)",		"happy"),

	S(";)",         "wink"),
	S(";-)",        "wink"),

	S(":D",			"laugh"),
	S(":-D",		"laugh"),
	S("lol",		"laugh"),
	S("LOL",		"laugh"),
	S(":lol:",		"laugh"),
	S(":LOL:",		"laugh"),
	S("*lol*",		"laugh"),
	S("*LOL*",		"laugh"),

	S("=)",			"interest"),
	S("=-)",		"interest"),

	S(":/",			"skeptic"),
	S(":-/",		"skeptic"),
	S(":\\",		"skeptic"),
	S(":-\\",		"skeptic"),

	S("<.<",		"lookside"),
	S("<_<",		"lookside"),
	S(">.>",		"lookside"),
	S(">_>",		"lookside"),

	S(":P",			"tongue"),
	S(":-P",		"tongue"),
	S(":p",			"tongue"),
	S(":-p",		"tongue"),

	S(":-x",		"sealed"),
	S(":-X",		"sealed"),

	S(":-|",		"disgust"),

	S(":(",         "sad"),
	S(":-(",        "sad"),

	S(":[",         "sadq"),
	S(":-[",        "sadq"),

	S(":cry:",		"cry"),
	S(":'-(",		"cry"),
	S(":~-(",		"cry"),
	S(":~(~~~",		"cry"),
	S(":~(~~",		"cry"),
	S(":~(~",		"cry"),
	S(":,(",		"cry"),
	S(";(",			"cry"),
	S(";-(",		"cry"),
	S("&.(..",		"cry"),
	S(":'(",		"cry"),
	S(":,-(",		"cry"),

	S(":o",			"omg"),
	S(":O",			"omg"),
	S(":-o",		"omg"),
	S(":-O",		"omg"),

	S(":love:",		"love"),
	S("(*_*)",		"love"),
	S("<*_*>",		"love"),
	S(":kiss:",		"love"),
	S(":-*",		"love"),

	S("-_-",		"ph34r"),
	S(":ph34r:",	"ph34r"),
#undef S
};

void CHTRichEditCtrl::AddSmileys(LPCTSTR pszLine)
{
	int iPos = 0;
	LPCTSTR pszStart = pszLine;
	for (LPCTSTR psz = pszLine; *psz != _T('\0');) {
		bool bFoundSmiley = false;
		if (iPos == 0 || psz[-1] == _T(' ') || psz[-1] == _T('.')) {
			for (unsigned i = 0; i < _countof(s_apszSmileys); ++i) {
				if (_tcsncmp(psz, s_apszSmileys[i].pszSmiley, s_apszSmileys[i].iLen) == 0
					&& (psz[s_apszSmileys[i].iLen] == _T('\0')
						|| psz[s_apszSmileys[i].iLen] == _T('\r')
						|| psz[s_apszSmileys[i].iLen] == _T('\n')
						|| psz[s_apszSmileys[i].iLen] == _T(' '))) //
				{
					if (psz - pszStart > 0)
						ReplaceSel(CString(pszStart, (int)(psz - pszStart)));
					CHARFORMAT2 cf = {};
					cf.cbSize = (UINT)sizeof cf;
					cf.dwMask = CFM_COLOR;
					COLORREF bk = (GetSelectionCharFormat(cf) & CFM_BACKCOLOR) ? cf.crBackColor : m_crBackground;
					if (!InsertSmiley(s_apszSmileys[i].pszID, bk))
						ReplaceSel(s_apszSmileys[i].pszSmiley);
					psz += s_apszSmileys[i].iLen;
					iPos += s_apszSmileys[i].iLen;
					pszStart = psz;
					bFoundSmiley = true;
					break;
				}
			}
		}
		if (!bFoundSmiley) {
			psz = _tcsinc(psz);
			++iPos;
		}
	}
	if (*pszStart != _T('\0'))
		ReplaceSel(pszStart);
}

HBITMAP IconToBitmap(HICON hIcon, COLORREF crBackground, int cx = 16, int cy = 16)
{
	if (cx <= 0 || cy <= 0) {
		ICONINFO ii;
		if (!::GetIconInfo(hIcon, &ii))
			return NULL;
		BITMAP bmi;
		int iSize = ::GetObject(ii.hbmColor, sizeof bmi, &bmi);
		::DeleteObject(ii.hbmMask);
		::DeleteObject(ii.hbmColor);
		if ((size_t)iSize < sizeof bmi - sizeof bmi.bmBits)
			return NULL;
		cx = bmi.bmWidth;
		cy = bmi.bmHeight;
	}

	// Draw icon into bitmap for keeping any available transparency. Another way todo this
	// might be to use a metafile - would need to test.
	HBITMAP hBitmap = NULL;
	try {
		// Do *not* create a transparent bitmap
		//ULONG_PTR gdiplusToken = 0;
		//Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		//if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::OK)
		//{
		//	Gdiplus::Bitmap bmp(hIcon);
		//	Gdiplus::Color colorBackground(255, GetRValue(crBackground), GetGValue(crBackground), GetBValue(crBackground));
		//	bmp.GetHBITMAP(colorBackground, &hBitmap);
		//}
		//Gdiplus::GdiplusShutdown(gdiplusToken);

#ifdef USE_METAFILE
		{
			HDC hdc = ::GetDC(HWND_DESKTOP);
			int iWidthMM = ::GetDeviceCaps(hdc, HORZSIZE);
			int iHeightMM = ::GetDeviceCaps(hdc, VERTSIZE);
			int iWidthPels = ::GetDeviceCaps(hdc, HORZRES);
			int iHeightPels = ::GetDeviceCaps(hdc, VERTRES);
			RECT rcMF = { 0, 0, ((cx + 1) * iWidthMM * 100) / iWidthPels, ((cy + 1) * iHeightMM * 100) / iHeightPels };
			HDC hdcEnhMF = ::CreateEnhMetaFile(NULL, NULL, &rcMF, NULL);
			if (hdcEnhMF) {
				::SetBkColor(hdcEnhMF, crBackground);
				::DrawIconEx(hdcEnhMF, 0, 0, hIcon, cx, cy, 0, 0, DI_NORMAL);
				hBitmap = (HBITMAP)::CloseEnhMetaFile(hdcEnhMF);
			}
			::ReleaseDC(HWND_DESKTOP, hdc);
		}
#else
		CClientDC dcScreen(CWnd::GetDesktopWindow());
		CDC dcMem;
		if (dcMem.CreateCompatibleDC(&dcScreen)) {
			CBitmap bmp;
			if (bmp.CreateCompatibleBitmap(&dcScreen, cx, cy)) {
				CBitmap *pbmpOld = dcMem.SelectObject(&bmp);
				dcMem.FillSolidRect(0, 0, cx, cy, crBackground);
				::DrawIconEx(dcMem, 0, 0, hIcon, cx, cy, 0, 0, DI_NORMAL);
				dcMem.SelectObject(pbmpOld);
				hBitmap = (HBITMAP)bmp.Detach();
			}
		}
#endif
	} catch (CException *ex) {
		ASSERT(0);
		ex->Delete();
	}

	return hBitmap;
}

HBITMAP CHTRichEditCtrl::GetSmileyBitmap(LPCTSTR pszSmileyID, COLORREF bk)
{
	CString sSmileyID;
	sSmileyID.Format(_T("%s%08lx"), pszSmileyID, bk);
	void *pvData;
	if (sm_aSmileyBitmaps.Lookup(sSmileyID, pvData))
		return (HBITMAP)pvData;

	int cx = 16, cy = 16;
	CHARFORMAT cf;
	GetDefaultCharFormat(cf);
	if (cf.cbSize == sizeof cf && cf.dwMask & CFM_SIZE) {
		HDC hDC = ::GetDC(HWND_DESKTOP);
		int iPixelFontSize = abs(-::MulDiv(cf.yHeight, ::GetDeviceCaps(hDC, LOGPIXELSY), 20) / 72);
		::ReleaseDC(NULL, hDC);
		//Point	Pixel	Icon
		//10	13		14
		//11	14		14
		//12	16		18
		//14	18		24
		//16	21		24
		//18	24		24
		//20	26		32
		if (iPixelFontSize <= 14)
			;//cx = cy = 16;
		else if (iPixelFontSize <= 17)
			cx = cy = 18;
		else if (iPixelFontSize <= 25)
			cx = cy = 24;
		else
			cx = cy = 32;
	}

	static const CString &strResourceName(_T("Smiley_"));
	HICON hIcon = theApp.LoadIcon(strResourceName + pszSmileyID, cx, cy, LR_DEFAULTCOLOR);
	if (hIcon == NULL)
		return (HBITMAP)NULL;

	// Don't specify an icon size for the bitmap creation, we could use 'cx' and 'cy' only for
	// the built-in icons because we know their sizes, but if there is a skin active, the
	// icons (which can also be GIF images) can have any size.
	HBITMAP hBitmap = IconToBitmap(hIcon, bk, 0, 0);
	if (hBitmap != NULL)
		sm_aSmileyBitmaps.SetAt(sSmileyID, hBitmap);
	return hBitmap;
}

bool CHTRichEditCtrl::InsertSmiley(LPCTSTR pszSmileyID, COLORREF bk)
{
	HBITMAP hbmp = GetSmileyBitmap(pszSmileyID, bk);
	if (hbmp == NULL)
		return false;

	CBitmapDataObject *pbdo = new CBitmapDataObject(hbmp);
	CComPtr<IDataObject> pIDataObject;
	pIDataObject.Attach((IDataObject*)pbdo->GetInterface(&IID_IDataObject));

	CComPtr<IRichEditOle> pIRichEditOle;
	pIRichEditOle.Attach(GetIRichEditOle());
	if (!pIRichEditOle)
		return false;

	CComPtr<IOleClientSite> pIOleClientSite;
	pIRichEditOle->GetClientSite(&pIOleClientSite);
	if (!pIOleClientSite)
		return false;

	if (sm_pIStorageSmileys == NULL) {
		CComPtr<ILockBytes> pILockBytes;
		if (CreateILockBytesOnHGlobal(NULL, TRUE, &pILockBytes) != S_OK)
			return false;
		if (StgCreateDocfileOnILockBytes(pILockBytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &sm_pIStorageSmileys) != S_OK)
			return false;
	}

	FORMATETC FormatEtc;
#ifdef USE_METAFILE
	FormatEtc.cfFormat = CF_ENHMETAFILE;
#else
	FormatEtc.cfFormat = CF_BITMAP;
#endif
	FormatEtc.ptd = NULL;
	FormatEtc.dwAspect = DVASPECT_CONTENT;
	FormatEtc.lindex = -1;
#ifdef USE_METAFILE
	FormatEtc.tymed = TYMED_ENHMF;
#else
	FormatEtc.tymed = TYMED_GDI;
#endif

	CComPtr<IOleObject> pIOleObject;
	if (OleCreateStaticFromData(pIDataObject, __uuidof(pIOleObject), OLERENDER_FORMAT, &FormatEtc, pIOleClientSite, sm_pIStorageSmileys, (void **)&pIOleObject) != S_OK)
		return false;
	OleSetContainedObject(pIOleObject, TRUE);

	REOBJECT reobject = {};
	reobject.cbStruct = (DWORD)sizeof reobject;
	if (pIOleObject->GetUserClassID(&reobject.clsid) != S_OK)
		return false;
	reobject.cp = (LONG)REO_CP_SELECTION;
	reobject.dvaspect = DVASPECT_CONTENT;
	reobject.poleobj = pIOleObject;
	reobject.polesite = pIOleClientSite;
	reobject.pstg = sm_pIStorageSmileys;
	reobject.dwFlags = REO_BELOWBASELINE;

	return pIRichEditOle->InsertObject(&reobject) == S_OK;
}

bool CHTRichEditCtrl::AddCaptcha(HBITMAP hbmp)
{
	if (hbmp == NULL)
		return false;

	SetSel(GetWindowTextLength(), GetWindowTextLength());
	CBitmapDataObject *pbdo = new CBitmapDataObject(hbmp);
	CComPtr<IDataObject> pIDataObject;
	pIDataObject.Attach((IDataObject*)pbdo->GetInterface(&IID_IDataObject));

	CComPtr<IRichEditOle> pIRichEditOle;
	pIRichEditOle.Attach(GetIRichEditOle());
	if (!pIRichEditOle)
		return false;

	CComPtr<IOleClientSite> pIOleClientSite;
	pIRichEditOle->GetClientSite(&pIOleClientSite);
	if (!pIOleClientSite)
		return false;

	if (m_pIStorageCaptchas == NULL) {
		CComPtr<ILockBytes> pILockBytes;
		if (CreateILockBytesOnHGlobal(NULL, TRUE, &pILockBytes) != S_OK)
			return false;
		if (StgCreateDocfileOnILockBytes(pILockBytes, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &m_pIStorageCaptchas) != S_OK)
			return false;
	}

	FORMATETC FormatEtc;
#ifdef USE_METAFILE
	FormatEtc.cfFormat = CF_ENHMETAFILE;
#else
	FormatEtc.cfFormat = CF_BITMAP;
#endif
	FormatEtc.ptd = NULL;
	FormatEtc.dwAspect = DVASPECT_CONTENT;
	FormatEtc.lindex = -1;
#ifdef USE_METAFILE
	FormatEtc.tymed = TYMED_ENHMF;
#else
	FormatEtc.tymed = TYMED_GDI;
#endif

	CComPtr<IOleObject> pIOleObject;
	if (OleCreateStaticFromData(pIDataObject, __uuidof(pIOleObject), OLERENDER_FORMAT, &FormatEtc, pIOleClientSite, m_pIStorageCaptchas, (void **)&pIOleObject) != S_OK)
		return false;
	OleSetContainedObject(pIOleObject, TRUE);

	REOBJECT reobject = {};
	reobject.cbStruct = (DWORD)sizeof reobject;
	if (pIOleObject->GetUserClassID(&reobject.clsid) != S_OK)
		return false;
	reobject.cp = (LONG)REO_CP_SELECTION;
	reobject.dvaspect = DVASPECT_CONTENT;
	reobject.poleobj = pIOleObject;
	reobject.polesite = pIOleClientSite;
	reobject.pstg = m_pIStorageCaptchas;
	reobject.dwFlags = REO_BELOWBASELINE;

	if (pIRichEditOle->InsertObject(&reobject) != S_OK)
		return false;
	ReplaceSel(_T(""));

	return true;
}