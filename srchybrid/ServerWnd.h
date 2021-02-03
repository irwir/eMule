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
#include "ResizableLib\ResizableDialog.h"
#include "ServerListCtrl.h"
#include "IconStatic.h"
#include "RichEditCtrlX.h"
#include "ClosableTabCtrl.h"
#include "SplitterControl.h"

class CHTRichEditCtrl;
class CCustomAutoComplete;

class CServerWnd : public CResizableDialog
{
	DECLARE_DYNAMIC(CServerWnd)

	enum
	{
		IDD = IDD_SERVER
	};

	enum ELogPaneItems
	{
		PaneServerInfo = 0, // those are CTabCtrl item indices
		PaneLog = 1,
		PaneVerboseLog = 2
	};

public:
	explicit CServerWnd(CWnd *pParent = NULL);   // standard constructor
	virtual	~CServerWnd();
	void Localize();
	bool UpdateServerMetFromURL(const CString &strURL);
	void ToggleDebugWindow();
	void UpdateMyInfo();
	void UpdateLogTabSelection();
	void SaveAllSettings();
	bool SaveServerMetStrings();
	void ShowNetworkInfo();
	void UpdateControlsState();
	void ResetHistory();
	void PasteServerFromClipboard();
	bool AddServer(uint16 nPort, const CString &strAddress, const CString &strName = CString(), bool bShowErrorMB = true);
	CString GetMyInfoString();

	CServerListCtrl serverlistctrl;
	CHTRichEditCtrl	*servermsgbox;
	CHTRichEditCtrl	*logbox;
	CHTRichEditCtrl	*debuglog;
	CClosableTabCtrl StatusSelector;
	CSplitterControl m_wndSplitter;

private:
	void	DoResize(int delta);
	void	UpdateSplitterRange();
//	void	DoSplitResize(int delta);
//	void	ShowSplitWindow(bool bReDraw = false);
	void	InitSplitter();
	void	ReattachAnchors();

	CIconStatic m_ctrlNewServerFrm;
	CIconStatic m_ctrlUpdateServerFrm;
	CIconStatic m_ctrlMyInfoFrm;
	CImageList m_imlLogPanes;
	CString m_strClickNewVersion;
	CRichEditCtrlX m_MyInfo;
	HICON icon_srvlist;
	CHARFORMAT2 m_cfDef;
	CHARFORMAT2 m_cfBold;
	CCustomAutoComplete *m_pacServerMetURL;
	bool	debug;

protected:
	void SetAllIcons();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedAddserver();
	afx_msg void OnBnClickedUpdateServerMetFromUrl();
	afx_msg void OnBnClickedResetLog();
	afx_msg void OnBnConnect();
	afx_msg void OnTcnSelchangeTab3(LPNMHDR, LRESULT *pResult);
	afx_msg void OnEnLinkServerBox(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnSysColorChange();
	afx_msg void OnDDClicked();
	afx_msg void OnSvrTextChange();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnStnDblclickServlstIco();
	afx_msg void OnSplitterMoved(LPNMHDR pNMHDR, LRESULT*);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
};