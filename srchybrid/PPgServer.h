#pragma once

class CPPgServer : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgServer)

	enum
	{
		IDD = IDD_PPG_SERVER
	};

public:
	CPPgServer();
	virtual	~CPPgServer() = default;

	void Localize();

protected:
	void LoadSettings();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSettingsChange()					{ SetModified(); }
	afx_msg void OnBnClickedEditadr();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};