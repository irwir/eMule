#pragma once
#include "Preferences.h"

class CPPgProxy : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgProxy)

	enum
	{
		IDD = IDD_PPG_PROXY
	};

public:
	CPPgProxy();
	virtual	~CPPgProxy() = default;

	void Localize();

protected:
	ProxySettings proxy;

	void LoadSettings();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedEnableProxy();
	afx_msg void OnBnClickedEnableAuthentication();
	afx_msg void OnCbnSelChangeProxyType();
	afx_msg void OnEnChangeProxyName()			{ SetModified(TRUE); }
	afx_msg void OnEnChangeProxyPort()			{ SetModified(TRUE); }
	afx_msg void OnEnChangeUserName()			{ SetModified(TRUE); }
	afx_msg void OnEnChangePassword()			{ SetModified(TRUE); }
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};