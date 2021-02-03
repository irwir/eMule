#pragma once

class TabControl : public CTabCtrl
{
public:
	TabControl();
	virtual	~TabControl();

	UINT GetLastMovementSource() const			{ return m_nSrcTab; }
	UINT GetLastMovementDestionation() const	{ return m_nDstTab; }

	BOOL ReorderTab(unsigned int nSrcTab, unsigned int nDstTab);
	void SetTabTextColor(int index, DWORD color);

protected:
	bool m_bDragging;     // Specifies that whether drag 'n drop is in progress.
	UINT m_nSrcTab;       // Specifies the source tab that is going to be moved.
	UINT m_nDstTab;       // Specifies the destination tab (drop position).
	bool m_bHotTracking;  // Specifies the state of whether the tab control has hot tracking enabled.
	CRect m_InsertPosRect;
	CPoint m_lclickPoint;
	CSpinButtonCtrl *m_pSpinCtrl;


	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);
	BOOL DragDetectPlus(CWnd *Handle, CPoint p);
	bool DrawIndicator(CPoint point);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd*);
};