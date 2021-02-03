#pragma once

/////////////////////////////////////////////////////////////////////////////
// CGradientStatic window

class CGradientStatic : public CStatic
{
// Construction
public:
	CGradientStatic();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGradientStatic)
	//}}AFX_VIRTUAL

// Implementation
public:
	void SetFont(CFont *pFont);
	virtual	~CGradientStatic();

	void SetInit(bool bInit)				{ m_bInit = bInit;		 }
	void SetHorizontal(bool bHorz = true)	{ m_bHorizontal = bHorz; }
	void SetInvert(bool bInvert = false)	{ m_bInvert = bInvert;	 }
	void SetColors(COLORREF crText, COLORREF crLB, COLORREF crRT)
	{
		m_crTextColor = crText;
		m_crColorLB = crLB;
		m_crColorRT = crRT;
	}

	// Generated message map functions
protected:
	bool m_bInit;
	bool m_bHorizontal;
	bool m_bInvert;

	COLORREF m_crColorLB;
	COLORREF m_crColorRT;
	COLORREF m_crTextColor;

	CFont m_cfFont;

	struct _TAG_GRADIENTSTATIC_MEM_
	{
		CDC		dc;
		CBitmap bmp;
		CBitmap *pold;
		int		cx;
		int		cy;

	} m_Mem;

	//{{AFX_MSG(CGradientStatic)
	afx_msg void OnPaint();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
private:
	void DrawVerticalText(CRect *pRect);
	void DrawHorizontalText(CRect *pRect);
	void DrawVerticalGradient();
	void DrawHorizontalGradient();
	void CreateGradient(CDC *pDC, CRect *pRect);
};