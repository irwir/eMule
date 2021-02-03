#pragma once

/////////////////////////////////////////////////////////////////////////////
// CIconStatic

class CIconStatic : public CStatic
{
	DECLARE_DYNAMIC(CIconStatic)
public:
	CIconStatic() = default;
	virtual	~CIconStatic();

	void SetIcon(LPCTSTR lpszIconID);
	void SetWindowText(LPCTSTR lpszText);

protected:
	CStatic m_wndPicture;
	CString m_strIconID;
	CString m_strText;
	CBitmap m_MemBMP;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSysColorChange();
};