#pragma once

class CPPgGeneral : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgGeneral)

	enum
	{
		IDD = IDD_PPG_GENERAL
	};
	void SetLangSel();
public:
	CPPgGeneral();
	virtual	~CPPgGeneral() = default;

	void Localize();

protected:
	CComboBox m_language;
	void LoadSettings();
	void UpdateEd2kLinkFixCtrl();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnSettingsChange()			{ SetModified(); }
	afx_msg void OnBnClickedEd2kfix();
	afx_msg void OnBnClickedEditWebservices();
	afx_msg void OnLangChange();
	afx_msg void OnBnClickedCheck4Update();
//	afx_msg void OnCbnCloseupLangs();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};