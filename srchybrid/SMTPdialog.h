#pragma once

class CSMTPserverDlg : public CDialog
{
	DECLARE_DYNAMIC(CSMTPserverDlg)

	enum
	{
		IDD = IDD_SMTPSERVER
	};
public:
	explicit CSMTPserverDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CSMTPserverDlg();

	void Localize();

protected:
	HICON m_icoWnd;
	EmailSettings m_mail;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnCbnSelChangeSecurity();
	afx_msg void OnCbnSelChangeAuthMethod();
};