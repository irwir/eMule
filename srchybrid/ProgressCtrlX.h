#pragma once

///////////////////////////////////////////////////////////////////////////////
// class CProgressCtrlX
//
// Author:  Yury Goltsman
// email:   ygprg@go.to
// page:    http://go.to/ygprg
// Copyright © 2000, Yury Goltsman
//
// This code provided "AS IS," without warranty of any kind.
// You may freely use or modify this code provided this
// Copyright is included in all derived versions.
//
// version : 1.1
// Added multi-color gradient
// Added filling with brush for background and bar(overrides color settings)
// Added borders attribute
// Added vertical text support
// Added snake mode
// Added reverse mode
// Added dual color for text
// Added text formatting
// Added tied mode for text and rubber bar mode
// Added support for vertically oriented control (PBS_VERTICAL)
//
// version : 1.0
//

#ifndef UIBITS_API
#ifdef UIBITS_DLL
#define  UIBITS_API __declspec(dllexport)
#else
#define  UIBITS_API __declspec(dllimport)
#endif
#endif

// To set text format use "SetTextFormat" and "HideText"
#define PBS_SHOW_PERCENT         0x0100
#define PBS_SHOW_POSITION        0x0200
#define PBS_SHOW_TEXTONLY        0x0300
#define PBS_TEXTMASK             0x0300

// To set this attributes use ModifyStyle() or appropriated functions
#define PBS_TIED_TEXT            0x1000
#define PBS_RUBBER_BAR           0x2000
#define PBS_REVERSE              0x4000
#define PBS_SNAKE                0x8000

/////////////////////////////////////////////////////////////////////////////
// class CProgressCtrlX

class /*UIBITS_API*/ CProgressCtrlX : public CProgressCtrl
{
public:
	// Construction
	CProgressCtrlX();

	// Attributes
	void SetGradientColors(COLORREF clrStart, COLORREF clrEnd)	{ m_ardwGradColors.SetSize(2); m_ardwGradColors.SetAt(0, clrStart); m_ardwGradColors.SetAt(1, clrEnd); }
	void GetGradientColors(COLORREF &clrStart, COLORREF &clrEnd) { clrStart = m_ardwGradColors[0]; clrEnd = m_ardwGradColors[1]; }

	void SetGradientColorsX(int nCount, COLORREF clrFirst, COLORREF clrNext, ...);
	const CDWordArray &GetGradientColorsX()						{ return m_ardwGradColors; }

	void SetBkColor(COLORREF clrBk)								{ m_clrBk = clrBk; }
	COLORREF GetBkColor()										{ return m_clrBk; }

	void SetTextColor(COLORREF clrTextOnBar, COLORREF clrTextOnBk = CLR_NONE) { m_clrTextOnBar = m_clrTextOnBk = clrTextOnBar; if (clrTextOnBk != CLR_NONE) m_clrTextOnBk = clrTextOnBk; }
	COLORREF GetTextColor()										{ return m_clrTextOnBar; }
	COLORREF GetTextColorOnBk()									{ return m_clrTextOnBk; }

	void SetShowPercent(BOOL fShowPercent = TRUE)				{ SetTextFormat(fShowPercent ? _T("%d%%") : NULL, PBS_SHOW_PERCENT); }
	BOOL GetShowPercent()										{ return GetStyle() & PBS_SHOW_PERCENT; }

	void SetTextFormat(LPCTSTR szFormat, DWORD ffFormat = PBS_SHOW_TEXTONLY);
	void HideText()												{ SetTextFormat(0); }

	void SetTiedText(BOOL fTiedText = TRUE)						{ ModifyStyle(fTiedText ? 0 : PBS_TIED_TEXT, fTiedText ? PBS_TIED_TEXT : 0, SWP_DRAWFRAME); }
	BOOL GetTiedText()											{ return GetStyle() & PBS_TIED_TEXT; }

	void SetRubberBar(BOOL fRubberBar = TRUE)					{ ModifyStyle(fRubberBar ? 0 : PBS_RUBBER_BAR, fRubberBar ? PBS_RUBBER_BAR : 0, SWP_DRAWFRAME); }
	BOOL GetRubberBar()											{ return GetStyle() & PBS_RUBBER_BAR; }

	void SetReverse(BOOL fReverse = TRUE)						{ ModifyStyle(fReverse ? 0 : PBS_REVERSE, fReverse ? PBS_REVERSE : 0, SWP_DRAWFRAME); }
	BOOL GetReverse()											{ return GetStyle() & PBS_REVERSE; }

	void SetSnake(BOOL fSnake = TRUE)							{ ModifyStyle(fSnake ? 0 : PBS_SNAKE | PBS_RUBBER_BAR, fSnake ? PBS_SNAKE | PBS_RUBBER_BAR : 0, SWP_DRAWFRAME); }
	BOOL GetSnake()												{ return GetStyle() & PBS_SNAKE; }

	void SetSnakeTail(int nTailSize)							{ m_nTailSize = nTailSize; }
	int  GetSnakeTail()											{ return m_nTailSize; }

	void SetBorders(const CRect &rcBorders)						{ m_rcBorders = rcBorders; }
	const CRect &GetBorders()									{ return m_rcBorders; }

	void SetEmpty(bool bVal, bool bRefresh = false)				{ m_bEmpty = bVal; if (bRefresh) Invalidate(); }

	// Operations

	// Overrides
		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CProgressCtrlX)
		//}}AFX_VIRTUAL

	// Implementation
	virtual	~CProgressCtrlX() = default;

protected:
	struct CDrawInfo
	{
		CDC *pDC;
		CRect rcClient;
		DWORD dwStyle;
		int nCurPos;
		int nLower;
		int nUpper;
	};

	virtual void DrawMultiGradient(const CDrawInfo &info, const CRect &rcGrad, const CRect &rcClip);
	virtual void DrawGradient(const CDrawInfo &info, const CRect &rcGrad, const CRect &rcClip, COLORREF clrStart, COLORREF clrEnd);
	virtual void DrawText(const CDrawInfo &info, const CRect &rcMax, const CRect &rcBar);
	virtual void DrawClippedText(const CDrawInfo &info, const CRect &rcClip, CString &sText, const CPoint &ptWndOrg);
	CRect ConvertToReal(const CDrawInfo &info, const CRect &rcVirt);
	virtual BOOL SetSnakePos(int &nOldPos, int nNewPos, BOOL fIncrement = FALSE);

	CRect m_rcBorders;
	// colour attributes
	CDWordArray m_ardwGradColors;
	COLORREF m_clrBk;
	COLORREF m_clrTextOnBar;
	COLORREF m_clrTextOnBk;

	// snake attributes
	int		 m_nTail;
	int		 m_nTailSize;
	int		 m_nStep;

	bool	 m_bEmpty;

	// Generated message map functions
	//{{AFX_MSG(CProgressCtrlX)
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnPaint();
	afx_msg LRESULT OnSetBarColor(WPARAM, LPARAM);
	afx_msg LRESULT OnSetBkColor(WPARAM, LPARAM);
	afx_msg LRESULT OnSetPos(WPARAM, LPARAM);
	afx_msg LRESULT OnDeltaPos(WPARAM, LPARAM);
	afx_msg LRESULT OnStepIt(WPARAM, LPARAM);
	afx_msg LRESULT OnSetStep(WPARAM, LPARAM);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};