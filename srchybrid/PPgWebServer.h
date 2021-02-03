#pragma once

class CPPgWebServer : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgWebServer)

	enum
	{
		IDD = IDD_PPG_WEBSRV
	};
	long m_generating;
	bool m_bNewCert;

public:
	CPPgWebServer();
	virtual	~CPPgWebServer() = default;

	void Localize();
	void SetUPnPState();

protected:
	BOOL m_bModified;
	HICON m_icoBrowse;

	void LoadSettings();

	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	void SetModified(BOOL bChanged = TRUE)
	{
		m_bModified = bChanged;
		CPropertyPage::SetModified(bChanged);
	}

	DECLARE_MESSAGE_MAP()
	afx_msg void OnEnChangeWSEnabled();
	afx_msg void OnChangeHTTPS();
	afx_msg void OnReloadTemplates();
	afx_msg void OnBnClickedTmplbrowse();
	afx_msg void OnGenerateCertificate();
	afx_msg void OnBnClickedCertbrowse();
	afx_msg void OnBnClickedKeybrowse();
	afx_msg void OnHelp();
	afx_msg void SetTmplButtonState();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnDataChange();
	afx_msg void OnDestroy();
};