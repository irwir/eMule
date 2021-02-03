// ------------------------------------------------------------
//  CDialogMinTrayBtn template class
//  MFC CDialog with minimize to system tray button (0.04)
//  Supports WinXP styles (thanks to David Yuheng Zhao for CVisualStylesXP - yuheng_zhao@yahoo.com)
// ------------------------------------------------------------
//  DialogMinTrayBtn.h
//  zegzav - 2002,2003 - eMule project (http://www.emule-project.net)
// ------------------------------------------------------------
#pragma once
#define HTMINTRAYBUTTON	65

template <class BASE= CDialog> class CDialogMinTrayBtn : public BASE
{
public:
	// constructor
	CDialogMinTrayBtn();
	explicit CDialogMinTrayBtn(LPCTSTR lpszTemplateName, CWnd *pParentWnd = NULL);
	explicit CDialogMinTrayBtn(UINT nIDTemplate, CWnd *pParentWnd = NULL);

	// methods
	void MinTrayBtnShow();
	void MinTrayBtnHide();
	inline BOOL MinTrayBtnIsVisible() const			{ return m_bMinTrayBtnVisible; }

	void MinTrayBtnEnable();
	void MinTrayBtnDisable();
	inline BOOL MinTrayBtnIsEnabled() const			{ return m_bMinTrayBtnEnabled; }

	void SetWindowText(LPCTSTR lpszString);

protected:
	// messages
	virtual BOOL OnInitDialog();
	afx_msg void OnNcPaint();
	afx_msg BOOL OnNcActivate(BOOL bActive);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint point);
	afx_msg void OnNcRButtonDown(UINT nHitTest, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT _OnThemeChanged();
	DECLARE_MESSAGE_MAP()

private:
	// internal methods
	void MinTrayBtnInit();
	void MinTrayBtnDraw();
	BOOL MinTrayBtnHitTest(CPoint ptScreen) const;
	void MinTrayBtnUpdatePosAndSize();

	void MinTrayBtnSetUp();
	void MinTrayBtnSetDown();

	inline const CPoint &MinTrayBtnGetPos() const	{ return m_MinTrayBtnPos; }
	inline const CSize &MinTrayBtnGetSize() const	{ return m_MinTrayBtnSize; }
	inline CRect MinTrayBtnGetRect() const			{ return CRect(MinTrayBtnGetPos(), MinTrayBtnGetSize()); }

	BOOL IsWindowsClassicStyle() const;
	INT GetVisualStylesXPColor() const;

	BOOL MinTrayBtnInitBitmap();

	// data members
	CPoint m_MinTrayBtnPos;
	CSize  m_MinTrayBtnSize;
	BOOL   m_bMinTrayBtnVisible;
	BOOL   m_bMinTrayBtnEnabled;
	BOOL   m_bMinTrayBtnUp;
	BOOL   m_bMinTrayBtnCapture;
	BOOL   m_bMinTrayBtnActive;
	BOOL   m_bMinTrayBtnHitTest;
	UINT_PTR m_nMinTrayBtnTimerId;
	CBitmap m_bmMinTrayBtnBitmap;
	BOOL	m_bMinTrayBtnWindowsClassicStyle;
	static const TCHAR *m_pszMinTrayBtnBmpName[];
};