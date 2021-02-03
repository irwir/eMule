#pragma once
#include "GDIThread.h"

class CCreditsThread : public CGDIThread
{
public:
	DECLARE_DYNAMIC(CCreditsThread)
	CCreditsThread(CWnd *pWnd, HDC hDC, LPCRECT rectScreen);

// Attributes
public:
	CRect		m_rectScreen;
	CRgn		m_rgnScreen;

	int			m_nScrollPos;

	// background bitmap
	CDC			m_dcBk;
	CBitmap		m_bmpBk;
	CBitmap		*m_pbmpOldBk;

	// credits bitmap
	CDC			m_dcCredits;
	CBitmap		m_bmpCredits;
	CBitmap		*m_pbmpOldCredits;

	// screen bitmap
	CDC			m_dcScreen;
	CBitmap		m_bmpScreen;
	CBitmap		*m_pbmpOldScreen;

	// mask bitmap
	CDC			m_dcMask;
	CBitmap		m_bmpMask;
	CBitmap		*m_pbmpOldMask;

	int			m_nCreditsBmpWidth;
	int			m_nCreditsBmpHeight;

	CArray<CString>				m_arCredits;
	CArray<COLORREF, COLORREF>	m_arColors;
	CArray<CFont*, CFont*>		m_arFonts;
	CArray<int, int>			m_arFontHeights;

// Operations
public:
	int CalcCreditsHeight();
	void InitText();
	void InitColors();
	void InitFonts();
	void CreateCredits();
	virtual BOOL InitInstance();
	virtual void SingleStep();
	void PaintBk(CDC *pDC);

// Implementation
protected:
	virtual	~CCreditsThread() = default;

	DECLARE_MESSAGE_MAP()
};
