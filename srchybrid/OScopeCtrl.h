#pragma once

/////////////////////////////////////////////////////////////////////////////
// COScopeCtrl window

class COScopeCtrl : public CWnd
{
public:
	explicit COScopeCtrl(int NTrends = 1);
	virtual	~COScopeCtrl();

	virtual BOOL Create(DWORD dwStyle, const CRect &rect, CWnd *pParentWnd, UINT nID = NULL);

	void SetTrendRatio(int iTrend, unsigned iRatio = 1);
	void SetLegendLabel(const CString &string, int iTrend);
	void SetBarsPlot(bool BarsPlot, int iTrend);
	void SetRange(double dLower, double dUpper, int iTrend = 0);
	void SetRanges(double dLower, double dUpper);
	void SetXUnits(const CString &string, const CString &XMin = CString(), const CString &XMax = CString());
	void SetYUnits(const CString &string, const CString &YMin = CString(), const CString &YMax = CString());
	void SetGridColor(COLORREF color);
	void SetPlotColor(COLORREF color, int iTrend = 0);
	COLORREF GetPlotColor(int iTrend = 0);
	void SetBackgroundColor(COLORREF color);
	void GetPlotRect(CRect &rPlotRect)		{ rPlotRect = m_rectPlot; }

	void AppendPoints(double dNewPoint[], bool bInvalidate = true, bool bAdd2List = true, bool bUseTrendRatio = true);
	void AppendEmptyPoints(double dNewPoint[], bool bInvalidate = true, bool bAdd2List = true, bool bUseTrendRatio = true);
	void InvalidateCtrl(bool deleteGraph = true);
	void DrawPoint();
	void Reset();
	int ReCreateGraph();

	bool ready;
	bool drawBars;
	bool autofitYscale;
	int m_nXGrids;
	int m_nYGrids;
	int m_nXPartial;
	int m_nShiftPixels;         // amount to shift with each new point
	int m_nTrendPoints;			// when you set this to > 0, then plot will
	int m_nMaxPointCnt;
	int m_nYDecimals;

	typedef struct m_str_struct
	{
		CString XUnits, XMin, XMax;
		CString YUnits, YMin, YMax;
	} m_str_t;
	m_str_t m_str;

	COLORREF m_crBackColor;        // background color
	COLORREF m_crGridColor;        // grid color

protected:
	int m_NTrends;

	struct CustShiftStruct  // when m_nTrendPoints > 0, this structure will contain needed vars
	{
		int m_nRmndr;		// remainder after dividing m_nWidthToDo/m_nPointsToDo
		int m_nWidthToDo;
		int m_nPointsToDo;
	} CustShift;

	typedef struct PlotDataStruct
	{
		double	dCurrentPosition;    // current position
		double	dPreviousPosition;   // previous position
		double	dLowerLimit;         // lower bounds
		double	dUpperLimit;         // upper bounds
		double	dRange;				// = UpperLimit - LowerLimit
		double	dVerticalFactor;
		CList<double> lstPoints;
		CPen	penPlot;
		CString	LegendLabel;
		COLORREF crPlotColor;       // data plot color
		int		nPrevY;
		// Optional variable to set a ratio for a given "trend".
		// The purpose here is to better implement the customizable
		// active connections ratio, so that points are redrawn correctly
		// when the ratio is changed, rather than having all previous points
		// no longer match up with the new ratio.
		int		iTrendRatio;
		bool	BarsPlot;
	} PlotData_t;
	PlotData_t *m_PlotData;

	int m_nClientHeight;
	int m_nClientWidth;
	int m_nPlotHeight;
	int m_nPlotWidth;

	CRect	m_rectClient;
	CRect	m_rectPlot;
	CDC     m_dcGrid;
	CDC     m_dcPlot;
	CBitmap m_bitmapOldGrid;
	CBitmap m_bitmapOldPlot;
	CBitmap m_bitmapGrid;
	CBitmap m_bitmapPlot;

	static CFont	sm_fontAxis;
	static LOGFONT	sm_logFontAxis;

	CPoint m_ptLastMousePos;
	UINT_PTR m_nRedrawTimer;
	UINT m_uLastMouseFlags;
	bool m_bDoUpdate;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnSysColorChange();
};