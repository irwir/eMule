/**************CSplitterControl interface***********
*	Class name :CSplitterControl
*	Purpose: Implement splitter control for dialog
*			or any other windows.
*	Author: Nguyen Huy Hung, Vietnamese student.
*	Date:	May 29th 2002.
*	Note: You can use it for any purposes. Feel free
*			to change, modify, but please do not
*			remove this.
*	No warranty of any risks.
*	(:-)
*/
#pragma once

/////////////////////////////////////////////////////////////////////////////
// CSplitterControl window

#define CW_LEFTALIGN	1
#define CW_RIGHTALIGN	2
#define CW_TOPALIGN		3
#define CW_BOTTOMALIGN	4

#define SPS_VERTICAL	1
#define SPS_HORIZONTAL	2

typedef struct SPC_NMHDR
{
	NMHDR hdr;
	int delta;
} SPC_NMHDR;

class CSplitterControl : public CStatic
{
public:
	CSplitterControl();
	virtual	~CSplitterControl() = default;

	void Create(DWORD dwStyle, const CRect &rect, CWnd *pParent, UINT nID);
	int GetStyle();
	int	SetStyle(int nStyle = SPS_VERTICAL);
	void SetRange(int nMin, int nMax);
	void SetRange(int nSubtraction, int nAddition, int nRoot);
	void SetDrawBorder(bool bEnable = true);

	static void ChangePos(CWnd *pWnd, int dx, int dy);
	static void ChangeWidth(CWnd *pWnd, int dx, DWORD dwFlag = CW_LEFTALIGN);
	static void ChangeHeight(CWnd *pWnd, int dy, DWORD dwFlag = CW_TOPALIGN);

protected:
	static HCURSOR m_hcurMoveVert;
	static HCURSOR m_hcurMoveHorz;
	BOOL	m_bIsPressed;	// Mouse is pressed down or not?
	int		m_nType;
	int		m_nX, m_nY;
	int		m_nMin, m_nMax;	// Min and Max range of the splitter.
	int		m_nSavePos;		// Save point on the lbutton down message
	bool	m_bDrawBorder;

	void MoveWindowTo(CPoint pt);
	virtual void DrawLine(CDC *pDC, int x, int y);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
};
