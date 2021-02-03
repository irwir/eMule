#pragma once

/////////////////////////////////////////////////////////////////////////////
// CTrayMenuBtn window

class CTrayMenuBtn : public CWnd
{
public:
	CTrayMenuBtn();
	virtual	~CTrayMenuBtn();

	CSize	m_sIcon;
	CString m_strText;
	CFont	m_cfFont;
	HICON	m_hIcon;
	UINT	m_nBtnID;
	bool	m_bBold;
	bool	m_bMouseOver;
	bool	m_bNoHover;
	bool	m_bUseIcon;
	bool	m_bParentCapture;

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
};