#pragma once

class CEditX : public CEdit
{
public:
	CEditX();

protected:
	DWORD m_dwLastDblClick;
	DWORD m_dwThirdClickTime;
	void UpdateMetrics();

	virtual void PreSubclassWindow();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
};
