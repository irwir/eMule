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
#include "ToolTipCtrlX.h"
#include "OtherFunctions.h"
#include "emule.h"
#include "log.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DFLT_DRAWTEXT_FLAGS	(DT_NOPREFIX | DT_EXTERNALLEADING | DT_END_ELLIPSIS)


IMPLEMENT_DYNAMIC(CToolTipCtrlX, CToolTipCtrl)

BEGIN_MESSAGE_MAP(CToolTipCtrlX, CToolTipCtrl)
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETTINGCHANGE()
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnNmCustomDraw)
	ON_NOTIFY_REFLECT(NM_THEMECHANGED, OnNmThemeChanged)
	ON_NOTIFY_REFLECT_EX(TTN_SHOW, OnTTShow)
END_MESSAGE_MAP()

CToolTipCtrlX::CToolTipCtrlX()
	: m_bCol1Bold(true)
	, m_bShowFileIcon()
{
	ResetSystemMetrics();
	m_dwCol1DrawTextFlags = m_dwCol2DrawTextFlags = DT_LEFT | DFLT_DRAWTEXT_FLAGS;
}

void CToolTipCtrlX::SetCol1DrawTextFlags(DWORD dwFlags)
{
	m_dwCol1DrawTextFlags = DFLT_DRAWTEXT_FLAGS | dwFlags;
}

void CToolTipCtrlX::SetCol2DrawTextFlags(DWORD dwFlags)
{
	m_dwCol2DrawTextFlags = DFLT_DRAWTEXT_FLAGS | dwFlags;
}

BOOL CToolTipCtrlX::SubclassWindow(HWND hWnd)
{
	BOOL bResult = __super::SubclassWindow(hWnd);
	// Win98/Win2000: Preventive, turning off tooltip animations should help in getting around
	// some glitches when using customized tooltip drawing. Though, doesn't seem to help a lot.
	if (theApp.m_ullComCtrlVer <= MAKEDLLVERULL(5, 81, 0, 0))
		ModifyStyle(0, TTS_NOANIMATE | TTS_NOFADE);
	return bResult;
}

void CToolTipCtrlX::ResetSystemMetrics()
{
	m_fontBold.DeleteObject();
	m_fontNormal.DeleteObject();

	NONCLIENTMETRICS ncm;
	ncm.cbSize = (UINT)sizeof(NONCLIENTMETRICS);
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0))
		memcpy(&m_lfNormal, &ncm.lfStatusFont, sizeof(m_lfNormal));
	else
		memset(&m_lfNormal, 0, sizeof m_lfNormal);

	m_crTooltipBkColor = ::GetSysColor(COLOR_INFOBK);
	m_crTooltipTextColor = ::GetSysColor(COLOR_INFOTEXT);
	m_rcScreen = CRect(0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	m_iScreenWidth4 = m_rcScreen.Width() / 4;
}

void CToolTipCtrlX::OnNmThemeChanged(LPNMHDR, LRESULT *pResult)
{
	ResetSystemMetrics();
	*pResult = 0;
}

void CToolTipCtrlX::OnSysColorChange()
{
	ResetSystemMetrics();
	CToolTipCtrl::OnSysColorChange();
}

void CToolTipCtrlX::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	ResetSystemMetrics();
	CToolTipCtrl::OnSettingChange(uFlags, lpszSection);
}

void CToolTipCtrlX::CustomPaint(LPNMTTCUSTOMDRAW pNMCD)
{
	CWnd *pwnd = CWnd::FromHandle(pNMCD->nmcd.hdr.hwndFrom);
	CDC *pdc = CDC::FromHandle(pNMCD->nmcd.hdc);

	// Windows Vista (General)
	// -----------------------
	// *Need* to use (some aspects) of the 'TOOLTIP' theme to get typical Vista tooltips.
	//
	// Windows Vista *without* SP1
	// ---------------------------
	// The Vista 'TOOLTIP' theme offers a bold version of the standard tooltip font via
	// the TTP_STANDARDTITLE part id. Furthermore TTP_STANDARDTITLE is the same font as
	// the standard tooltip font (TTP_STANDARD). So, the 'TOOLTIP' theme can get used
	// thoroughly.
	//
	// Windows Vista *with* SP1
	// ------------------------
	// The Vista SP1(!) 'TOOLTIP' theme does though *not* offer a bold font. Keep
	// in mind that TTP_STANDARDTITLE does not return a bold font. Keep also in mind
	// that TTP_STANDARDTITLE is even a *different* font than TTP_STANDARD!
	// Which means, that TTP_STANDARDTITLE should *not* be used within the same line
	// as TTP_STANDARD. It just looks weird. TTP_STANDARDTITLE could be used for a
	// single line (the title line), but it would though not give us a bold font.
	// So, actually we can use the Vista 'TOOLTIP' theme only for getting the proper
	// tooltip background.
	//
	// Windows XP
	// ----------
	// Can *not* use the 'TOOLTIP' theme at all because it would give us only a (non-bold)
	// black font on a white tooltip window background. Seems that the 'TOOLTIP' theme under
	// WinXP is just using the default Window values (black+white) and does not
	// use any of the tooltip specific Window metrics...
	//
	bool bUseEmbeddedThemeFonts = false;
	HTHEME hTheme = NULL;
	if (theApp.IsVistaThemeActive()) {
		hTheme = g_xpStyle.OpenThemeData(*pwnd, L"TOOLTIP");
		// Using the theme's fonts works only under Vista without SP1. When SP1
		// is installed the fonts which are used for TTP_STANDARDTITLE and TTP_STANDARD
		// do no longer match (should not get displayed within the same text line).
		if (hTheme)
			bUseEmbeddedThemeFonts = true;
	}

	CString strText;
	pwnd->GetWindowText(strText);

	RECT rcWnd;
	pwnd->GetWindowRect(&rcWnd);

	CFont *pOldDCFont = NULL;
	if (hTheme == NULL || !bUseEmbeddedThemeFonts) {
		// If we have a theme, we try to query the correct fonts from it
		if (m_fontNormal.m_hObject == NULL && hTheme) {
			// Windows Vista Ultimate, 6.0.6001 SP1 Build 6001, English with German Language Pack
			// ----------------------------------------------------------------------------------
			// TTP_STANDARD, TTSS_NORMAL
			//  Height   -12
			//  Weight   400
			//  CharSet  1
			//  Quality  5
			//  FaceName "Segoe UI"
			//
			// TTP_STANDARDTITLE, TTSS_NORMAL
			//  Height   -12
			//  Weight   400
			//  CharSet  1
			//  Quality  5
			//  FaceName "Segoe UI Fett" (!!!) Note: "Fett" (that is German !?)
			//
			// That font name ("Segoe UI *Fett*") looks quite suspicious. I would not be surprised
			// if that eventually leads to a problem within the font mapper which may not be capable
			// of finding the (English) version of that font and thus falls back to the standard
			// system font. At least this would explain why the TTP_STANDARD and TTP_STANDARDTITLE
			// fonts are *different* on that particular Windows Vista system.
			LOGFONT lf = {};
			if (g_xpStyle.GetThemeFont(hTheme, pdc->m_hDC, TTP_STANDARD, TTSS_NORMAL, TMT_FONT, &lf) == S_OK) {
				VERIFY(m_fontNormal.CreateFontIndirect(&lf));

				if (m_bCol1Bold && m_fontBold.m_hObject == NULL) {
					memset(&lf, 0, sizeof lf);
					if (g_xpStyle.GetThemeFont(hTheme, pdc->m_hDC, TTP_STANDARDTITLE, TTSS_NORMAL, TMT_FONT, &lf) == S_OK)
						VERIFY(m_fontBold.CreateFontIndirect(&lf));
				}

				// Get the tooltip font color
				COLORREF crText;
				if (g_xpStyle.GetThemeColor(hTheme, TTP_STANDARD, TTSS_NORMAL, TMT_TEXTCOLOR, &crText) == S_OK)
					m_crTooltipTextColor = crText;
			}
		}

		// If needed, create the standard tooltip font which was queried from the system metrics
		if (m_fontNormal.m_hObject == NULL && m_lfNormal.lfHeight != 0)
			VERIFY(m_fontNormal.CreateFontIndirect(&m_lfNormal));

		// Select the tooltip font
		if (m_fontNormal.m_hObject != NULL) {
			pOldDCFont = pdc->SelectObject(&m_fontNormal);

			// If needed, create the bold version of the tooltip font by deriving it from the standard font
			if (m_bCol1Bold && m_fontBold.m_hObject == NULL) {
				LOGFONT lf;
				m_fontNormal.GetLogFont(&lf);
				lf.lfWeight = FW_BOLD;
				VERIFY(m_fontBold.CreateFontIndirect(&lf));
			}
		}

		// Set the font color (queried from system metrics or queried from theme)
		pdc->SetTextColor(m_crTooltipTextColor);
	}

	// Auto-format the text only if explicitly requested. Otherwise we would also format
	// single line tooltips for regular list items, and if those list items contain ':'
	// characters they would be shown partially in bold. For performance reasons, the
	// auto-format is to be requested by appending the TOOLTIP_AUTOFORMAT_SUFFIX_CH
	// character. Appending, because we can remove that character efficiently without
	// re-allocating the entire string.
	bool bAutoFormatText = (strText.Right(1)[0] == TOOLTIP_AUTOFORMAT_SUFFIX_CH);
	if (bAutoFormatText)
		strText.Truncate(strText.GetLength() - 1); // truncate the TOOLTIP_AUTOFORMAT_SUFFIX_CH char
	bool bShowFileIcon = m_bShowFileIcon && bAutoFormatText;
	if (bShowFileIcon) {
		int iPosNL = strText.Find(_T('\n'));
		if (iPosNL > 0) {
			int iPosColon = strText.Find(_T(':'));
			if (iPosColon < iPosNL)
				bShowFileIcon = false; // 1st line does not contain a filename
		}
	}

	int iTextHeight = 0;
	int iMaxCol1Width = 0;
	int iMaxCol2Width = 0;
	int iMaxSingleLineWidth = 0;
	int iCaptionHeight = 0;
	static const int iLineHeightOff = 1;
	const int iCaptionEnd = bShowFileIcon ? maxi(strText.Find(_T("\n<br_head>\n")), 0) : 0; // special tooltip with file icon
	const int iIconMinYBorder = bShowFileIcon ? 3 : 0;
	const int iIconWidth = bShowFileIcon ? theApp.GetBigSytemIconSize().cx : 0;
	const int iIconHeight = bShowFileIcon ? theApp.GetBigSytemIconSize().cy : 0;
	const int iIconDrawingWidth = bShowFileIcon ? (iIconWidth + 9) : 0;
	CSize siz, sizText;
	CRect rcExtent;
	static const RECT rcBounding = { 0, 0, 32767, 32767 };
	for (int iPos = 0; iPos >= 0;) {
		const CString &strLine(GetNextString(strText, _T('\n'), iPos));
		int iColon = bAutoFormatText ? strLine.Find(_T(':')) : -1;
		if (iColon >= 0) {
			if (hTheme && bUseEmbeddedThemeFonts) {
				g_xpStyle.GetThemeTextExtent(hTheme, *pdc, m_bCol1Bold ? TTP_STANDARDTITLE : TTP_STANDARD, TTSS_NORMAL, strLine, iColon + 1, m_dwCol1DrawTextFlags, &rcBounding, &rcExtent);
				siz.cx = rcExtent.Width();
				siz.cy = rcExtent.Height();
			} else {
				CFont *pOldFont = m_bCol1Bold ? pdc->SelectObject(&m_fontBold) : NULL;
				siz = pdc->GetTextExtent(strLine, iColon + 1);
				if (pOldFont)
					pdc->SelectObject(pOldFont);
			}
			iMaxCol1Width = maxi(iMaxCol1Width, (int)(siz.cx + ((bShowFileIcon && iPos <= iCaptionEnd + strLine.GetLength()) ? iIconDrawingWidth : 0)));
			iTextHeight = siz.cy + iLineHeightOff; // update height with 'col1' string, because 'col2' string might be empty and therefore has no height
			if (iPos <= iCaptionEnd)
				iCaptionHeight += siz.cy + iLineHeightOff;
			else
				sizText.cy += siz.cy + iLineHeightOff;

			LPCTSTR pszCol2 = (LPCTSTR)strLine + iColon + 1;
			while (_istspace(*pszCol2))
				++pszCol2;
			if (*pszCol2 != _T('\0')) {
				if (hTheme && bUseEmbeddedThemeFonts) {
					g_xpStyle.GetThemeTextExtent(hTheme, *pdc, TTP_STANDARD, TTSS_NORMAL, pszCol2, (int)(((LPCTSTR)strLine + strLine.GetLength()) - pszCol2), m_dwCol2DrawTextFlags, &rcBounding, &rcExtent);
					siz.cx = rcExtent.Width();
					siz.cy = rcExtent.Height();
				} else
					siz = pdc->GetTextExtent(pszCol2, (int)(((LPCTSTR)strLine + strLine.GetLength()) - pszCol2));

				iMaxCol2Width = max(iMaxCol2Width, siz.cx);
			}
		} else if (bShowFileIcon && iPos <= iCaptionEnd && iPos == strLine.GetLength() + 1) {
			// file name, printed bold on top without any tabbing or desc
			if (hTheme && bUseEmbeddedThemeFonts) {
				g_xpStyle.GetThemeTextExtent(hTheme, *pdc, m_bCol1Bold ? TTP_STANDARDTITLE : TTP_STANDARD, TTSS_NORMAL, strLine, strLine.GetLength(), m_dwCol1DrawTextFlags, &rcBounding, &rcExtent);
				siz.cx = rcExtent.Width();
				siz.cy = rcExtent.Height();
			} else {
				CFont *pOldFont = m_bCol1Bold ? pdc->SelectObject(&m_fontBold) : NULL;
				siz = pdc->GetTextExtent(strLine);
				if (pOldFont)
					pdc->SelectObject(pOldFont);
			}
			iMaxSingleLineWidth = max(iMaxSingleLineWidth, siz.cx + iIconDrawingWidth);
			iCaptionHeight += siz.cy + iLineHeightOff;
		} else if (!strLine.IsEmpty() && strLine.Compare(_T("<br>")) != 0 && strLine.Compare(_T("<br_head>")) != 0) {
			if (hTheme && bUseEmbeddedThemeFonts) {
				g_xpStyle.GetThemeTextExtent(hTheme, *pdc, TTP_STANDARD, TTSS_NORMAL, strLine, strLine.GetLength(), m_dwCol2DrawTextFlags, &rcBounding, &rcExtent);
				siz.cx = rcExtent.Width();
				siz.cy = rcExtent.Height();
			} else
				siz = pdc->GetTextExtent(strLine);

			iMaxSingleLineWidth = maxi(iMaxSingleLineWidth, (int)(siz.cx + ((bShowFileIcon && iPos <= iCaptionEnd) ? iIconDrawingWidth : 0)));
			if (bShowFileIcon && iPos <= iCaptionEnd + strLine.GetLength())
				iCaptionHeight += siz.cy + iLineHeightOff;
			else
				sizText.cy += siz.cy + iLineHeightOff;
		} else {
			if (hTheme && bUseEmbeddedThemeFonts) {
				g_xpStyle.GetThemeTextExtent(hTheme, *pdc, TTP_STANDARD, TTSS_NORMAL, _T(" "), 1, m_dwCol2DrawTextFlags, &rcBounding, &rcExtent);
				siz.cx = rcExtent.Width();
				siz.cy = rcExtent.Height();
			} else {
				// TODO: Would need to use 'GetTabbedTextExtent' here, but do we actually use 'tabbed' text here at all ??
				siz = pdc->GetTextExtent(_T(" "), 1);
			}
			sizText.cy += siz.cy + iLineHeightOff;
		}
	}
	if (bShowFileIcon && iCaptionEnd > 0)
		iCaptionHeight = maxi(iCaptionHeight, (int)(theApp.GetBigSytemIconSize().cy + (2 * iIconMinYBorder)));
	sizText.cy += iCaptionHeight;
	if (hTheme && theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0))
		sizText.cy += 2; // extra bottom margin for Vista Theme

	iMaxCol1Width = min(m_iScreenWidth4, iMaxCol1Width);
	iMaxCol2Width = min(m_iScreenWidth4 * 2, iMaxCol2Width);

	static const int iMiddleMargin = 6;
	iMaxSingleLineWidth = maxi(iMaxSingleLineWidth, iMaxCol1Width + iMiddleMargin + iMaxCol2Width);
	iMaxSingleLineWidth = min(m_iScreenWidth4 * 3, iMaxSingleLineWidth);
	sizText.cx = iMaxSingleLineWidth;

	if (pNMCD->uDrawFlags & DT_CALCRECT) {
		pNMCD->nmcd.rc.left = rcWnd.left;
		pNMCD->nmcd.rc.top = rcWnd.top;
		pNMCD->nmcd.rc.right = rcWnd.left + sizText.cx;
		pNMCD->nmcd.rc.bottom = rcWnd.top + sizText.cy;
	} else {
		pwnd->ScreenToClient(&rcWnd);

		int iOldBkColor = -1;
		if (hTheme) {
			int iPartId = TTP_STANDARD;
			int iStateId = TTSS_NORMAL;
			if (g_xpStyle.IsThemeBackgroundPartiallyTransparent(hTheme, iPartId, iStateId))
				g_xpStyle.DrawThemeParentBackground(m_hWnd, pdc->m_hDC, &rcWnd);
			g_xpStyle.DrawThemeBackground(hTheme, pdc->m_hDC, iPartId, iStateId, &rcWnd, NULL);
		} else {
			::FillRect(*pdc, &rcWnd, ::GetSysColorBrush(COLOR_INFOBK));
			iOldBkColor = pdc->SetBkColor(m_crTooltipBkColor);
			// Vista: Need to draw the window border explicitly !?
			if (theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) {
				CPen pen;
				pen.CreatePen(0, 1, m_crTooltipTextColor);
				CPen *pOP = pdc->SelectObject(&pen);
				pdc->MoveTo(rcWnd.left, rcWnd.top);
				pdc->LineTo(rcWnd.right - 1, rcWnd.top);
				pdc->LineTo(rcWnd.right - 1, rcWnd.bottom - 1);
				pdc->LineTo(rcWnd.left, rcWnd.bottom - 1);
				pdc->LineTo(rcWnd.left, rcWnd.top);
				pdc->SelectObject(pOP);
				pen.DeleteObject();
			}
		}

		int iOldBkMode = 0;
		if ((hTheme && !bUseEmbeddedThemeFonts) || (hTheme == NULL && iOldBkColor != -1))
			iOldBkMode = pdc->SetBkMode(TRANSPARENT);

		CPoint ptText(pNMCD->nmcd.rc.left, pNMCD->nmcd.rc.top);

		for (int iPos = 0; iPos >= 0;) {
			const CString &strLine(GetNextString(strText, _T('\n'), iPos));
			int iColon = bAutoFormatText ? strLine.Find(_T(':')) : -1;
			CRect rcDT;
			if (!bShowFileIcon || (unsigned)iPos > (unsigned)iCaptionEnd + strLine.GetLength())
				rcDT.SetRect(ptText.x, ptText.y, ptText.x + iMaxCol1Width, ptText.y + iTextHeight);
			else
				rcDT.SetRect(ptText.x + iIconDrawingWidth, ptText.y, ptText.x + iMaxCol1Width, ptText.y + iTextHeight);
			if (iColon >= 0) {
				// don't draw empty <col1> strings (they are still handy to use for skipping the <col1> space)
				if (iColon > 0)
					if (hTheme && bUseEmbeddedThemeFonts)
						g_xpStyle.DrawThemeText(hTheme, pdc->m_hDC, m_bCol1Bold ? TTP_STANDARDTITLE : TTP_STANDARD, TTSS_NORMAL, strLine, iColon + 1, m_dwCol1DrawTextFlags, 0, &rcDT);
					else {
						CFont *pOldFont = m_bCol1Bold ? pdc->SelectObject(&m_fontBold) : NULL;
						pdc->DrawText(strLine, iColon + 1, &rcDT, m_dwCol1DrawTextFlags);
						if (pOldFont)
							pdc->SelectObject(pOldFont);
					}

				LPCTSTR pszCol2 = (LPCTSTR)strLine + iColon + 1;
				while (_istspace(*pszCol2))
					++pszCol2;
				if (*pszCol2 != _T('\0')) {
					rcDT.left = ptText.x + iMaxCol1Width + iMiddleMargin;
					rcDT.right = rcDT.left + iMaxCol2Width;
					if (hTheme && bUseEmbeddedThemeFonts)
						g_xpStyle.DrawThemeText(hTheme, pdc->m_hDC, TTP_STANDARD, TTSS_NORMAL, pszCol2, (int)(((LPCTSTR)strLine + strLine.GetLength()) - pszCol2), m_dwCol2DrawTextFlags, 0, &rcDT);
					else
						pdc->DrawText(pszCol2, (int)(((LPCTSTR)strLine + strLine.GetLength()) - pszCol2), &rcDT, m_dwCol2DrawTextFlags);
				}

				ptText.y += iTextHeight;
			} else if (bShowFileIcon && iPos <= iCaptionEnd && iPos == strLine.GetLength() + 1) {
				const CRect rect(ptText.x + iIconDrawingWidth, ptText.y, ptText.x + iMaxSingleLineWidth, ptText.y + iTextHeight);
				// first line on special file icon tab - draw icon and bold filename
				if (hTheme && bUseEmbeddedThemeFonts)
					g_xpStyle.DrawThemeText(hTheme, pdc->m_hDC, m_bCol1Bold ? TTP_STANDARDTITLE : TTP_STANDARD, TTSS_NORMAL, strLine, strLine.GetLength(), m_dwCol1DrawTextFlags, 0, &rect);
				else {
					CFont *pOldFont = m_bCol1Bold ? pdc->SelectObject(&m_fontBold) : NULL;
					pdc->DrawText(strLine, const_cast<CRect&>(rect), m_dwCol1DrawTextFlags);
					if (pOldFont)
						pdc->SelectObject(pOldFont);
				}

				ptText.y += iTextHeight;
				int iImage = theApp.GetFileTypeSystemImageIdx(strLine, -1, true);
				if (theApp.GetBigSystemImageList() != NULL) {
					int iPosY = rcDT.top;
					if (iCaptionHeight > iIconHeight)
						iPosY += (iCaptionHeight - iIconHeight) / 2;
					::ImageList_Draw(theApp.GetBigSystemImageList(), iImage, pdc->GetSafeHdc(), ptText.x, iPosY, ILD_TRANSPARENT);
				}
			} else {
				bool bIsBrHeadLine = !bAutoFormatText || (strLine != _T("<br>"));
				if (!bIsBrHeadLine || (bIsBrHeadLine = (strLine == _T("<br_head>"))) == true) {
					CPen pen;
					pen.CreatePen(0, 1, m_crTooltipTextColor);
					CPen *pOP = pdc->SelectObject(&pen);
					if (bIsBrHeadLine)
						ptText.y = iCaptionHeight;
					pdc->MoveTo(ptText.x, ptText.y + ((iTextHeight - 2) / 2));
					pdc->LineTo(ptText.x + iMaxSingleLineWidth, ptText.y + ((iTextHeight - 2) / 2));
					ptText.y += iTextHeight;
					pdc->SelectObject(pOP);
					pen.DeleteObject();
				} else {
					if (hTheme && bUseEmbeddedThemeFonts) {
						const RECT rcLine = { ptText.x, ptText.y, 32767, 32767 };
						g_xpStyle.DrawThemeText(hTheme, pdc->m_hDC, TTP_STANDARD, TTSS_NORMAL, strLine, strLine.GetLength(), DT_EXPANDTABS | m_dwCol2DrawTextFlags, 0, &rcLine);
						ptText.y += iTextHeight;
					} else {
						// Text is written in the currently selected font. If 'nTabPositions' is 0 and 'lpnTabStopPositions' is NULL,
						// tabs are expanded to eight times the average character width.
						// Win98: To draw an empty line we need to output at least a space.
						siz = pdc->TabbedTextOut(ptText.x, ptText.y, (strLine.IsEmpty() ? _T(" ") : strLine), -1, NULL, 0);
						ptText.y += siz.cy + iLineHeightOff;
					}
				}
			}
		}
		if (iOldBkColor != -1)
			pdc->SetBkColor(iOldBkColor);
		if (hTheme && !bUseEmbeddedThemeFonts)
			pdc->SetBkMode(iOldBkMode);
	}
	if (pOldDCFont)
		pdc->SelectObject(pOldDCFont);
	if (hTheme)
		g_xpStyle.CloseThemeData(hTheme);
}

void EnsureWindowVisible(const RECT &rcScreen, RECT &rc)
{
	// 1st: Move the window towards the left side, in case it exceeds the desktop width.
	// 2nd: Move the window towards the right side, in case it exceeds the desktop width.
	//
	// This order of actions ensures that in case the window is too large for the desktop,
	// the user will though see the top-left part of the window.
	if (rc.right > rcScreen.right) {
		rc.left -= rc.right - rcScreen.right;
		rc.right = rcScreen.right;
	}
	if (rc.left < rcScreen.left) {
		rc.right += rcScreen.left - rc.left;
		rc.left = rcScreen.left;
	}

	// Same logic for bottom/top
	if (rc.bottom > rcScreen.bottom) {
		rc.top -= rc.bottom - rcScreen.bottom;
		rc.bottom = rcScreen.bottom;
	}
	if (rc.top < rcScreen.top) {
		rc.bottom += rcScreen.top - rc.top;
		rc.top = rcScreen.top;
	}
}

BOOL CToolTipCtrlX::OnTTShow(LPNMHDR pNMHDR, LRESULT *pResult)
{
	//DebugLog(_T("OnTTShow: rcScreen: %d,%d-%d,%d: styles=%08x  exStyles=%08x"), m_rcScreen, GetStyle(), ::GetWindowLongPtr(m_hWnd, GWL_EXSTYLE));

	// Win98/Win2000: The only chance to resize a tooltip window is to do it within the TTN_SHOW notification.
	if (theApp.m_ullComCtrlVer <= MAKEDLLVERULL(5, 81, 0, 0)) {
		NMTTCUSTOMDRAW nmttcd = {};
		nmttcd.uDrawFlags = DT_NOPREFIX | DT_CALCRECT | DT_EXTERNALLEADING | DT_EXPANDTABS | DT_WORDBREAK;
		nmttcd.nmcd.hdr = *pNMHDR;
		nmttcd.nmcd.dwDrawStage = CDDS_PREPAINT;
		nmttcd.nmcd.hdc = ::GetDC(pNMHDR->hwndFrom);
		CustomPaint(&nmttcd);
		::ReleaseDC(pNMHDR->hwndFrom, nmttcd.nmcd.hdc);

		CRect rcWnd(nmttcd.nmcd.rc);
		AdjustRect(&rcWnd);

		// Win98/Win2000: We have to explicitly ensure that the tooltip window remains within the visible desktop window.
		EnsureWindowVisible(m_rcScreen, rcWnd);

		// Win98/Win2000: The only chance to resize a tooltip window is to do it within the TTN_SHOW notification.
		// Win98/Win2000: Must *not* specify 'SWP_NOZORDER' - some of the tooltip windows may get drawn behind(!) the application window!
		::SetWindowPos(pNMHDR->hwndFrom, NULL, rcWnd.left, rcWnd.top, rcWnd.Width(), rcWnd.Height(), SWP_NOACTIVATE /*| SWP_NOZORDER*/);

		*pResult = TRUE; // Windows API: Suppress default positioning
		return TRUE;	 // MFC API:     Suppress further routing of this message
	}

	// If the TTN_SHOW notification is not sent to the sub-classed tooltip control, we would lose the
	// exact positioning of in-place tooltips which is performed by the tooltip control by default.
	// Thus it is important that we tell MFC (not the Windows API in that case) to further route this message.
	*pResult = FALSE;	// Windows API: Perform default positioning
	return FALSE;		// MFC API.     Perform further routing of this message (to the sub-classed tooltip control)
}

void CToolTipCtrlX::OnNmCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMTTCUSTOMDRAW pNMCD = reinterpret_cast<LPNMTTCUSTOMDRAW>(pNMHDR);

	// For each tooltip which is to be shown Windows invokes the draw function at least 2 times.
	//	1st invocation: to get the drawing rectangle
	//	2nd invocation: to draw the actual tooltip window contents
	//
	// 'DrawText' flags for the 1st and 2nd/3rd call
	// ---------------------------------------------
	// NMTTCUSTOMDRAW 00000e50	DT_NOPREFIX | DT_CALCRECT | DT_EXTERNALLEADING | DT_EXPANDTABS | DT_WORDBREAK
	// TTN_SHOW
	// NMTTCUSTOMDRAW 00000a50	DT_NOPREFIX |               DT_EXTERNALLEADING | DT_EXPANDTABS | DT_WORDBREAK
	// --an additional NMTTCUSTOMDRAW may follow which is identical to the 2nd--
	// NMTTCUSTOMDRAW 00000a50	DT_NOPREFIX |               DT_EXTERNALLEADING | DT_EXPANDTABS | DT_WORDBREAK

	//if (pNMCD->nmcd.dwDrawStage == CDDS_PREPAINT && pNMCD->uDrawFlags & DT_CALCRECT)
	//	DebugLog(_T(""));
	//DebugLog(_T("OnNmCustomDraw: DrawFlags=%08x DrawStage=%08x ItemSpec=%08x ItemState=%08x ItemlParam=%08x rc=%4d,%4d,(%4dx%4d), styles=%08x  exStyles=%08x"), pNMCD->uDrawFlags, pNMCD->nmcd.dwDrawStage, pNMCD->nmcd.dwItemSpec, pNMCD->nmcd.uItemState, pNMCD->nmcd.lItemlParam, pNMCD->nmcd.rc.left, pNMCD->nmcd.rc.top, pNMCD->nmcd.rc.right - pNMCD->nmcd.rc.left, pNMCD->nmcd.rc.bottom - pNMCD->nmcd.rc.top, GetStyle(), ::GetWindowLongPtr(m_hWnd, GWL_EXSTYLE));

	if (theApp.m_ullComCtrlVer <= MAKEDLLVERULL(5, 81, 0, 0)) {
		// Win98/Win2000: Resize and position the tooltip window in TTN_SHOW.
		// Win98/Win2000: Customize the tooltip window in CDDS_POSTPAINT.
		if (pNMCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
			// PROBLEM: Windows will draw the default tooltip during the non-DT_CALCRECT cycle,
			// and if the system is very slow (or when using remote desktop), the default tooltip
			// may be visible for a second. However, we need to draw the customized tooltip after
			// CDDS_POSTPAINT otherwise it won't be visible at all.

			// Cheap solution: Let windows draw the text with the background color, so the glitch
			// is not that much visible.
			SetTextColor(pNMCD->nmcd.hdc, m_crTooltipBkColor);
			*pResult = CDRF_NOTIFYPOSTPAINT;
			return;
		}
		if (pNMCD->nmcd.dwDrawStage == CDDS_POSTPAINT) {
			CustomPaint(pNMCD);
			*pResult = CDRF_SKIPDEFAULT;
			return;
		}
	} else {
		// XP/Vista: Resize, position and customize the tooltip window all in 'CDDS_PREPAINT'.
		if (pNMCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
			CustomPaint(pNMCD);
			*pResult = CDRF_SKIPDEFAULT;
			return;
		}
	}
	*pResult = CDRF_DODEFAULT;
}