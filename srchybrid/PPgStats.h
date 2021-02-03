#pragma once
#include "ColorButton.h"

class CPPgStats : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgStats)

	enum
	{
		IDD = IDD_PPG_STATS
	};

public:
	CPPgStats();
	virtual	~CPPgStats() = default;

	void Localize();

protected:
	int m_iStatsColors;
	DWORD *m_pdwStatsColors;
	CComboBox m_colors;
	CComboBox m_cratio;
	CColorButton m_ctlColor;
	CSliderCtrl m_ctlGraphsUpdate;
	CSliderCtrl m_ctlGraphsAvgTime;
	CSliderCtrl m_ctlStatsUpdate;
	int m_iGraphsUpdate;
	int m_iGraphsAvgTime;
	int m_iStatsUpdate;
	BOOL m_bModified;
	bool m_bFillGraphs;

	void ShowInterval();
	void SetModified(BOOL bChanged = TRUE);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnCbnSelChangeColorSelector();
	afx_msg LRESULT OnColorPopupSelChange(WPARAM, LPARAM);
	afx_msg void OnEnChangeCGraphScale()			{ SetModified(); }
	afx_msg void OnCbnSelChangeCRatio()				{ SetModified(); }
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedFillGraphs();
};