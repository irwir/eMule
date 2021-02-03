/********************************************************************
*
* Copyright (c) 2002 Sven Wiegand <mail@sven-wiegand.de>
*
* You can use this and modify this in any way you want,
* BUT LEAVE THIS HEADER INTACT.
*
* Redistribution is appreciated.
*
* $Workfile:$
* $Revision:$
* $Modtime:$
* $Author:$
*
* Revision History:
*	$History:$
*
*********************************************************************/
#pragma once
#include "TreePropSheetPgFrame.h"

/**
An implementation of CPropPageFrame, that works well for Windows XP
styled systems and older windows versions (without themes).

@author Sven Wiegand
*/
class /*AFX_EXT_CLASS*/ CPropPageFrameDefault : public CWnd, public CPropPageFrame
{
// construction/destruction
public:
	CPropPageFrameDefault() = default;
	virtual	~CPropPageFrameDefault();

// operations

// overridings
	virtual BOOL Create(DWORD dwWindowStyle, const RECT &rect, CWnd *pwndParent, UINT nID);
	virtual CWnd* GetWnd();
	virtual void SetCaption(LPCTSTR lpszCaption, HICON hIcon = NULL);

protected:
	virtual CRect CalcMsgArea();
	virtual CRect CalcCaptionArea();
	virtual void DrawCaption(CDC *pDc, CRect rect, LPCTSTR lpszCaption, HICON hIcon);

// Implementation helpers
	/**
	Fills a rectangular area with a gradient color starting at the left
	side with the color clrLeft and ending at the right sight with the
	color clrRight.

	@param pDc
		Device context to draw the rectangle in.
	@param rect
		Rectangular area to fill.
	@param clrLeft
		Color on the left side.
	@param clrRight
		Color on the right side.
	*/
	void FillGradientRectH(CDC *pDc, const CRect &rect, COLORREF clrLeft, COLORREF clrRight);

	/**
	Returns TRUE if Windows XP theme support is available, FALSE
	otherwise.
	*/
//	BOOL ThemeSupport() const;

	//{{AFX_VIRTUAL(CPropPageFrameDefault)
	//}}AFX_VIRTUAL

// message handlers
	//{{AFX_MSG(CPropPageFrameDefault)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

// attributes
	/**
	Image list that contains only the current icon or nothing if there
	is no icon.
	*/
	CImageList m_Images;
};