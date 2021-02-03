#pragma once

class CPPgConnection : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgConnection)

	enum
	{
		IDD = IDD_PPG_CONNECTION
	};
	uint16 m_lastudp;
	void ChangePorts(uint8 iWhat); //0 - UDP, 1 - TCP, 2 - enable/disable "Test ports"
	bool ChangeUDP();

public:
	CPPgConnection();
	virtual	~CPPgConnection() = default;

	void Localize();
	void LoadSettings();

	static bool CheckUp(uint32 mUp, uint32 &mDown);
	static bool CheckDown(uint32 &mUp, uint32 mDown);
protected:
	CSliderCtrl m_ctlMaxDown;
	CSliderCtrl m_ctlMaxUp;

	void ShowLimitValues();
	void SetRateSliderTicks(CSliderCtrl &rRate);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnSettingsChange()				{ SetModified(); }
	afx_msg void OnEnChangeUDPDisable();
	afx_msg void OnLimiterChange();
	afx_msg void OnBnClickedWizard();
//	afx_msg void OnBnClickedNetworkKademlia();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnBnClickedOpenports();
	afx_msg void OnStartPortTest();
	afx_msg void OnEnKillFocusTCP();
	afx_msg void OnEnKillFocusUDP();
};