#pragma once

struct SSearchParams;
class CSearchResultsWnd;
class CSearchParamsWnd;
class CSearchFile;
class CClosableTabCtrl;


///////////////////////////////////////////////////////////////////////////////
// CSearchDlg frame

class CSearchDlg : public CFrameWnd
{
	DECLARE_DYNCREATE(CSearchDlg)

public:
	CSearchDlg();           // protected constructor used by dynamic creation
	virtual	~CSearchDlg();
	CSearchResultsWnd *m_pwndResults;

	BOOL Create(CWnd *pParent);

	void Localize();
	void CreateMenus();

	void RemoveResult(const CSearchFile *pFile);

	bool DoNewEd2kSearch(SSearchParams *pParams);
	bool DoNewKadSearch(SSearchParams *pParams);
	void CancelEd2kSearch();
	void CancelKadSearch(UINT uSearchID);
	void SetNextSearchID(uint32 uNextID);
	void ProcessEd2kSearchLinkRequest(const CString &strSearchTerm);

	bool CanSearchRelatedFiles() const;
	void SearchRelatedFiles(CPtrList &listFiles);

	void DownloadSelected();
	void DownloadSelected(bool bPaused);

	bool CanDeleteSearch() const;
	bool CanDeleteAllSearches() const;
	void DeleteSearch(uint32 nSearchID);
	void DeleteAllSearches();

	void LocalEd2kSearchEnd(UINT nCount, bool bMoreResultsAvailable);
	void AddEd2kSearchResults(UINT nCount);

	bool CreateNewTab(SSearchParams *pParams, bool bActiveIcon = true);
	SSearchParams* GetSearchParamsBySearchID(uint32 nSearchID);
//	void ShowSearchSelector(bool visible);
	CClosableTabCtrl& GetSearchSelector();

	int GetSelectedCat();
	void UpdateCatTabs();
	void SaveAllSettings();
//	BOOL SaveSearchStrings();
	void ResetHistory();

	void SetToolTipsDelay(UINT uDelay);
	void DeleteAllSearchListCtrlItems();

	BOOL IsSearchParamsWndVisible() const;
	void OpenParametersWnd();
	void DockParametersWnd();

	void UpdateSearch(CSearchFile *pSearchFile);

protected:
	CSearchParamsWnd	*m_pwndParams;

	virtual BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnClose();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};