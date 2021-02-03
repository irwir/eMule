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

#define TOOLTIP_AUTOFORMAT_SUFFIX _T("\a")
#define TOOLTIP_AUTOFORMAT_SUFFIX_CH	_T('\a')

class CToolTipCtrlX : public CToolTipCtrl
{
	DECLARE_DYNAMIC(CToolTipCtrlX)
public:
	CToolTipCtrlX();
	virtual	~CToolTipCtrlX() = default;

	void SetCol1DrawTextFlags(DWORD dwFlags);
	void SetCol2DrawTextFlags(DWORD dwFlags);
	void SetFileIconToolTip(bool bEnable)		{ m_bShowFileIcon = bEnable; }
	BOOL SubclassWindow(HWND hWnd);

protected:
	LOGFONT m_lfNormal;
	CFont m_fontBold;
	CFont m_fontNormal;
	CRect m_rcScreen;
	COLORREF m_crTooltipBkColor;
	COLORREF m_crTooltipTextColor;
	DWORD m_dwCol1DrawTextFlags;
	DWORD m_dwCol2DrawTextFlags;
	int m_iScreenWidth4;
	bool m_bCol1Bold;
	bool m_bShowFileIcon;

	void ResetSystemMetrics();
	void CustomPaint(LPNMTTCUSTOMDRAW);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnNmCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmThemeChanged(LPNMHDR, LRESULT *pResult);
	afx_msg BOOL OnTTShow(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnSysColorChange();
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
//	afx_msg void OnWindowPosChanging(WINDOWPOS *lpwndpos);
};