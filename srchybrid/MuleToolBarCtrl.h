#pragma once

#define IDC_TOOLBAR			16127
#define IDC_TOOLBARBUTTON	16129

#define	TBBTN_CONNECT	(IDC_TOOLBARBUTTON + 0)
#define	TBBTN_KAD		(IDC_TOOLBARBUTTON + 1)
#define	TBBTN_SERVER	(IDC_TOOLBARBUTTON + 2)
#define	TBBTN_TRANSFERS	(IDC_TOOLBARBUTTON + 3)
#define	TBBTN_SEARCH	(IDC_TOOLBARBUTTON + 4)
#define	TBBTN_SHARED	(IDC_TOOLBARBUTTON + 5)
#define	TBBTN_MESSAGES	(IDC_TOOLBARBUTTON + 6)
#define	TBBTN_IRC		(IDC_TOOLBARBUTTON + 7)
#define	TBBTN_STATS		(IDC_TOOLBARBUTTON + 8)
#define	TBBTN_OPTIONS	(IDC_TOOLBARBUTTON + 9)
#define	TBBTN_TOOLS		(IDC_TOOLBARBUTTON + 10)
#define	TBBTN_HELP		(IDC_TOOLBARBUTTON + 11)

#define	MULE_TOOLBAR_BAND_NR	0

enum EToolbarLabelType : uint8
{
	NoLabels	= 0,
	LabelsBelow = 1,
	LabelsRight = 2
};

class CMuleToolbarCtrl : public CToolBarCtrl
{
	DECLARE_DYNAMIC(CMuleToolbarCtrl)

public:
	CMuleToolbarCtrl();
	virtual	~CMuleToolbarCtrl();

	void Init();
	void Localize();
	void Refresh();
	void SaveCurHeight();
	void UpdateBackground();
	void PressMuleButton(int nID);
	BOOL GetMaxSize(LPSIZE pSize) const;

	static int GetDefaultLabelType()		{ return (int)LabelsBelow; }

protected:
	CSize		m_sizBtnBmp;
	int			m_iPreviousHeight;
	int			m_iLastPressedButton;
	int			m_buttoncount;
	TBBUTTON	TBButtons[12];
	TCHAR		TBStrings[12][200];
	CStringArray m_astrToolbarPaths;
	EToolbarLabelType m_eLabelType;
	CStringArray m_astrSkinPaths;
	CBitmap		m_bmpBack;

	void ChangeToolbarBitmap(const CString &path, bool bRefresh);
	void ChangeTextLabelStyle(EToolbarLabelType eLabelType, bool bRefresh, bool bForceUpdateButtons = false);
	void UpdateIdealSize();
	void SetAllButtonsStrings();
	void SetAllButtonsWidth();

#ifdef _DEBUG
	void Dump();
#endif

	void AutoSize();
	virtual	BOOL OnCommand(WPARAM wParam, LPARAM);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnNmRClick(LPNMHDR, LRESULT *pResult);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSysColorChange();
	afx_msg void OnTbnEndAdjust(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnTbnGetButtonInfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnTbnInitCustomize(LPNMHDR, LRESULT *pResult);
	afx_msg void OnTbnQueryDelete(LPNMHDR, LRESULT *pResult);
	afx_msg void OnTbnQueryInsert(LPNMHDR, LRESULT *pResult);
	afx_msg void OnTbnReset(LPNMHDR, LRESULT*);
	afx_msg void OnTbnToolbarChange(LPNMHDR, LRESULT *pResult);
};