#pragma once

/////////////////////////////////////////////////////////////////////////////
// CRichEditCtrlX window

class CRichEditCtrlX : public CRichEditCtrl
{
public:
	CRichEditCtrlX();
	virtual	~CRichEditCtrlX() = default;

	void SetDisableSelectOnFocus(bool bDisable = true);
	void SetSyntaxColoring(LPCTSTR *ppszKeywords = NULL, LPCTSTR pszSeparators = NULL);

	CRichEditCtrlX& operator<<(LPCTSTR psz);
	CRichEditCtrlX& operator<<(char *psz);
	CRichEditCtrlX& operator<<(UINT uVal);
	CRichEditCtrlX& operator<<(int iVal);
	CRichEditCtrlX& operator<<(double fVal);

	void SetRTFText(const CStringA &rstrTextA);

protected:
	bool m_bDisableSelectOnFocus;
	bool m_bSelfUpdate;
	bool m_bForceArrowCursor;
	HCURSOR m_hArrowCursor;
	CStringArray m_astrKeywords;
	CString m_strSeparators;
	CHARFORMAT2 m_cfDef;
	CHARFORMAT2 m_cfKeyword;

	void UpdateSyntaxColoring();
	static DWORD CALLBACK StreamInCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM);

	DECLARE_MESSAGE_MAP()
	afx_msg UINT OnGetDlgCode();
	afx_msg BOOL OnEnLink(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnEnChange();
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
};
