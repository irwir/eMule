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
#include "ResizableLib/ResizableDialog.h"
#include "IrcNickListCtrl.h"
#include "IrcChannelListCtrl.h"
#include "IrcChannelTabCtrl.h"
#include "SplitterControl.h"
#include "ToolBarCtrlX.h"

class CIrcMain;
class CSmileySelector;

class CIrcWnd : public CResizableDialog
{
	DECLARE_DYNAMIC(CIrcWnd)
	friend class CIrcChannelTabCtrl;

	enum
	{
		IDD = IDD_IRC
	};

	void SetFontStyle(TCHAR c);
public:
	explicit CIrcWnd(CWnd *pParent = NULL);
	virtual	~CIrcWnd();

	void Localize();
	bool GetLoggedIn() const;
	void SetLoggedIn(bool bFlag);
	void SetSendFileString(const CString &sInFile);
	const CString& GetSendFileString() const;
	bool IsConnected() const;
	void UpdateFonts(CFont *pFont);
	void ParseChangeMode(const CString &sChannel, const CString &sChanger, CString sCommands, const CString &sParams);
	void AddCurrent(const CString &sLine, bool bShowActivity = true, UINT uStatusCode = 0);
	void AddStatus(const CString &sLine, bool bShowActivity = true, UINT uStatusCode = 0);
	void AddStatusF(LPCTSTR sLine, ...); //to pass sLine by reference would be an 'undefined behaviour'
	void AddInfoMessage(Channel *pChannel, const CString &sLine);
	void AddInfoMessage(const CString &sChannel, const CString &sLine, const bool bShowChannel = false);
	void AddInfoMessageC(Channel *pChannel, const COLORREF &msgcolour, LPCTSTR sLine);
	void AddInfoMessageC(const CString &sChannel, const COLORREF &msgcolour, LPCTSTR sLine);
	void AddInfoMessageCF(const CString &sChannel, const COLORREF &msgcolour, LPCTSTR sLine, ...);
	void AddInfoMessageF(const CString &sChannel, LPCTSTR sLine, ...);
	void AddMessage(const CString &sChannel, const CString &sTargetName, const CString &sLine);
	void AddMessageF(const CString &sChannel, const CString &sTargetName, LPCTSTR sLine, ...);
	void AddColorLine(const CString &line, CHTRichEditCtrl &wnd, COLORREF crForeground = CLR_DEFAULT);
	void SetConnectStatus(bool bFlag);
	void NoticeMessage(const CString &sSource, const CString &sTarget, const CString &sMessage);
	CString StripMessageOfFontCodes(const CString &sTemp);
	CString StripMessageOfColorCodes(const CString &sTemp);
	void SetTopic(const CString &sChannel, const CString &sTopic);
	void SendString(const CString &sSend);
	void EnableSmileys(bool bEnable)				{ m_wndChanSel.EnableSmileys(bEnable); }
	afx_msg void OnBnClickedCloseChannel(int iItem = -1);
	static bool UpdateModes(const CString &sAllModes, CString &sModes, TCHAR cDir, TCHAR cCommand);

	CEdit m_wndInput;
	CIrcMain *m_pIrcMain;
	CIrcChannelTabCtrl m_wndChanSel;
	CIrcNickListCtrl m_wndNicks;
	CIrcChannelListCtrl m_wndChanList;
	CToolBarCtrlX m_wndFormat;
	static LPCTSTR const sBadCharsIRC; //IRC nick filter

protected:
	CString m_sSendString;
	CSplitterControl m_wndSplitterHorz;
	CSmileySelector *m_pwndSmileySel;
	bool m_bLoggedIn;
	bool m_bConnected;

	void OnChatTextChange();
	void DoResize(int iDelta);
	void UpdateChannelChildWindowsSize();
	void SetAllIcons();

	virtual BOOL OnInitDialog();
	virtual void OnSize(UINT uType, int iCx, int iCy);
	virtual int OnCreate(LPCREATESTRUCT lpCreateStruct);
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam,LPARAM lParam );
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual LRESULT DefWindowProc(UINT uMessage, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSysColorChange();
	afx_msg void OnBnClickedIrcConnect();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnBnClickedIrcSend();
	afx_msg LRESULT OnCloseTab(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnQueryTab(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedSmiley();
	afx_msg void OnBnClickedBold();
	afx_msg void OnBnClickedColour();
	afx_msg void OnBnClickedItalic();
	afx_msg void OnBnClickedUnderline();
	afx_msg void OnBnClickedReset();
	afx_msg LRESULT OnSelEndOK(WPARAM wParam, LPARAM);
	afx_msg LRESULT OnSelEndCancel(WPARAM wParam, LPARAM);
	afx_msg void OnEnRequestResizeTitle(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
};