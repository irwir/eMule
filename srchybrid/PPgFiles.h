#pragma once

class CPPgFiles : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgFiles)

	enum
	{
		IDD = IDD_PPG_FILES
	};

public:
	CPPgFiles();
	virtual	~CPPgFiles() = default;

	void Localize();

protected:
	CListBox m_uncfolders;
	HICON m_icoBrowse;

	void LoadSettings();
	void OnSettingsChangeCat(uint8 index);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSetCleanupFilter();
	afx_msg void BrowseVideoplayer();
	afx_msg void OnSettingsChange();
	afx_msg void OnSettingsChangeCat1()		{ OnSettingsChangeCat(1); }
	afx_msg void OnSettingsChangeCat2()		{ OnSettingsChangeCat(2); }
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnDestroy();
};