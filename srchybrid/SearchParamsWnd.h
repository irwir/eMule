#pragma once

#include "EditX.h"
#include "ComboBoxEx2.h"
#include "ListCtrlEditable.h"
class CCustomAutoComplete;

typedef enum
{
	orMinSize,
	orMaxSize,
	orAvailability,
	orCompleteSources,
	orExtension,
	orCodec,
	orBitrate,
	orLength,
	orTitle,
	orAlbum,
	orArtist
} EOptsRows;

class CSearchParamsWnd : public CDialogBar
{
	DECLARE_DYNAMIC(CSearchParamsWnd);

	enum
	{
		IDD = IDD_SEARCH_PARAMS
	};
	void WipeOptionFields();

// Construction
public:
	CSearchParamsWnd();
	virtual	~CSearchParamsWnd();

	CEditX m_ctlName;
	CButton m_ctlStart;
	CButton m_ctlCancel;
	CButton m_ctlMore;
	CSearchResultsWnd *m_searchdlg;

	void Localize();
	void ResetHistory();
	void SaveSettings();

	SSearchParams* GetParameters();
	void SetParameters(const SSearchParams *pParams);
	void ProcessEd2kSearchLinkRequest(const CString &strSearchTerm);

	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);
	virtual void OnUpdateCmdUI(CFrameWnd *pTarget, BOOL bDisableIfNoHndler);
	virtual BOOL PreTranslateMessage(MSG *pMsg);

protected:
	HCURSOR m_hcurMove;
	CComboBoxEx2 m_ctlMethod;
	CComboBoxEx2 m_ctlFileType;
	CEditableListCtrl m_ctlOpts;
	CRect m_rcNameLbl;
	CRect m_rcName;
	CRect m_rcDropDownArrow;
	CRect m_rcFileTypeLbl;
	CRect m_rcFileType;
	CRect m_rcReset;
	CRect m_rcMethodLbl;
	CRect m_rcMethod;
	CRect m_rcOpts;
	CRect m_rcStart;
	CRect m_rcMore;
	CRect m_rcCancel;
	CImageList m_imlSearchMethods;
	CImageList m_imlFileType;
	CSize m_szMRU;
	CSize m_szFloat;
	CCustomAutoComplete *m_pacSearchString;

	void UpdateControls();
	BOOL SaveSearchStrings();
	void SetAllIcons();
	void InitMethodsCtrl();
	void InitFileTypesCtrl();
	uint64 GetSearchAttrSize(const CString &rstrExpr);
	ULONG GetSearchAttrNumber(const CString &rstrExpr);
	ULONG GetSearchAttrLength(const CString &rstrExpr);

	virtual void DoDataExchange(CDataExchange *pDX);

	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnInitDialog(WPARAM, LPARAM);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedMore();
	afx_msg void OnCbnSelChangeMethod();
	afx_msg void OnCbnSelEndOkMethod();
	afx_msg void OnDDClicked();
	afx_msg void OnBnClickedSearchReset();
	afx_msg void OnEnChangeName();
	afx_msg void OnDestroy();
	afx_msg void OnSysColorChange();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};