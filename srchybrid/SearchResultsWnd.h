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
#include "ResizableLib\ResizableFormView.h"
#include "SearchListCtrl.h"
#include "ClosableTabCtrl.h"
#include "IconStatic.h"
#include "EditX.h"
#include "EditDelayed.h"
#include "ComboBoxEx2.h"
#include "ListCtrlEditable.h"

class CCustomAutoComplete;
class Packet;
class CSafeMemFile;
class CSearchParamsWnd;
struct SSearchParams;
class CDropDownButton;
class CButtonsTabCtrl;


///////////////////////////////////////////////////////////////////////////////
// CSearchResultsSelector

class CSearchResultsSelector : public CClosableTabCtrl
{
public:
	CSearchResultsSelector() = default;

protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd*, CPoint point);
};

///////////////////////////////////////////////////////////////////////////////
// CSearchResultsWnd dialog

class CSearchResultsWnd : public CResizableFormView
{
	DECLARE_DYNCREATE(CSearchResultsWnd)

	enum
	{
		IDD = IDD_SEARCH
	};
	void	NoTabItems();

public:
	explicit CSearchResultsWnd(CWnd *pParent = NULL);   // standard constructor
	virtual	~CSearchResultsWnd();
	CSearchResultsWnd(const CSearchResultsWnd&) = delete;
	CSearchResultsWnd& operator=(const CSearchResultsWnd&) = delete;

	CSearchListCtrl searchlistctrl;
	CSearchResultsSelector searchselect;
	CSearchParamsWnd *m_pwndParams;
	CStringArray m_astrFilter;

	void	Localize();

	void	StartSearch(SSearchParams *pParams);
	bool	SearchMore();
	void	CancelSearch(uint32 uSearchID = 0);

	bool	DoNewEd2kSearch(SSearchParams *pParams);
	void	CancelEd2kSearch();
	bool	IsLocalEd2kSearchRunning() const	{ return (m_uTimerLocalServer != 0); }
	bool	IsGlobalEd2kSearchRunning() const	{ return (global_search_timer != 0); }
	void	LocalEd2kSearchEnd(UINT count, bool bMoreResultsAvailable);
	void	AddEd2kSearchResults(UINT count);
	void	SetNextSearchID(uint32 uNextID)		{ m_nEd2kSearchID = uNextID; }
	uint32	GetNextSearchID()					{ return ++m_nEd2kSearchID; }

	bool	DoNewKadSearch(SSearchParams *pParams);
	void	CancelKadSearch(uint32 uSearchID);

	bool	CanSearchRelatedFiles() const;
	void	SearchRelatedFiles(CPtrList &listFiles);

	void	DownloadSelected();
	void	DownloadSelected(bool bPaused);

	bool	CanDeleteSearch() const;
	bool	CanDeleteAllSearches() const;
	void	DeleteSearch(uint32 uSearchID);
	void	DeleteAllSearches();
	void	DeleteSelectedSearch();

	bool	CreateNewTab(SSearchParams *pParams, bool bActiveIcon = true);
	void	ShowSearchSelector(bool visible);
	int		GetSelectedCat();
	void	UpdateCatTabs();

	SSearchParams* GetSearchResultsParams(uint32 uSearchID) const;

	uint32	GetFilterColumn() const				{ return m_nFilterColumn; }

protected:
	Packet		*m_searchpacket;
	bool		m_b64BitSearchPacket;
	UINT_PTR	global_search_timer;
	UINT_PTR	m_uTimerLocalServer;
	CProgressCtrl searchprogress;
	CHeaderCtrl m_ctlSearchListHeader;
	CEditDelayed m_ctlFilter;
	CButton		m_ctlOpenParamsWnd;
	bool		cancelled;
	uint16		servercount;
	bool		m_globsearch;
	uint32		m_nEd2kSearchID;
	CImageList	m_imlSearchResults;
	CButtonsTabCtrl	*m_cattabs;
	CDropDownButton	*m_btnSearchListMenu;
	int			m_iSentMoreReq;
	uint32		m_nFilterColumn;

	bool StartNewSearch(SSearchParams *pParams);
	void SearchStarted();
	void SearchCancelled(uint32 uSearchID);
	CString	CreateWebQuery(SSearchParams *pParams);
	void ShowResults(const SSearchParams *pParams);
	void SetAllIcons();
	void SetSearchResultsIcon(uint32 uSearchID, int iImage);
	void SetActiveSearchResultsIcon(uint32 uSearchID);
	void SetInactiveSearchResultsIcon(uint32 uSearchID);


	virtual void OnInitialUpdate();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDblClkSearchList(LPNMHDR, LRESULT *pResult);
	afx_msg void OnSelChangeTab(LPNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnCloseTab(WPARAM wParam, LPARAM);
	afx_msg LRESULT OnDblClickTab(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg void OnSysColorChange();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedDownloadSelected();
	afx_msg void OnBnClickedClearAll();
	afx_msg void OnClose();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg LRESULT OnIdleUpdateCmdUI(WPARAM, LPARAM);
	afx_msg void OnBnClickedOpenParamsWnd();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg LRESULT OnChangeFilter(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSearchListMenuBtnDropDown(LPNMHDR, LRESULT*);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
};