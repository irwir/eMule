#pragma once

class CConnectionWizardDlg : public CDialog
{
	DECLARE_DYNAMIC(CConnectionWizardDlg)

	enum
	{
		IDD = IDD_WIZARD
	};

public:
	explicit CConnectionWizardDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CConnectionWizardDlg();

	void Localize();

protected:
	HICON m_icoWnd;
	CListCtrl m_provider;
	int m_iOS;
	int m_iTotalDownload;

	void SetCustomItemsActivation();

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedWizHighdownloadRadio();
	afx_msg void OnBnClickedWizLowdownloadRadio();
	afx_msg void OnBnClickedWizMediumdownloadRadio();
	afx_msg void OnBnClickedWizRadioOsNtxp();
	afx_msg void OnBnClickedWizRadioUs98me();
	afx_msg void OnBnClickedWizResetButton();
	afx_msg void OnNmClickProviders(LPNMHDR, LRESULT *pResult);
};