//////////////////////////////////////////////////
// CMemoryDC - memory DC
//
// Author: Keith Rule
// Email:  keithr@europa.com
// Copyright 1996-2002, Keith Rule
//
// You may freely use or modify this code provided this
// Copyright is included in all derived versions.
//
// History - 10/3/97 Fixed scrolling bug.
//               Added print support. - KR
//
//       11/3/99 Fixed most common complaint. Added
//            background color fill. - KR
//
//       11/3/99 Added support for mapping modes other than
//            MM_TEXT as suggested by Lee Sang Hun. - KR
//
//       02/11/02 Added support for CScrollView as supplied
//             by Gary Kirkham. - KR
//
// This class implements a memory Device Context which allows
// flicker free drawing.
#pragma once

class CMemoryDC : public CDC
{
private:
	CBitmap		m_bitmap;		// Off-screen bitmap
	CBitmap		*m_oldBitmap;	// bitmap originally found in CMemoryDC
	CFont		*m_oldFont;
	CDC			*m_pDC;			// Saves CDC passed in constructor
	CRect		m_rect;			// Rectangle of drawing area.
	bool		m_bMemDC;		// TRUE if CDC really is a Memory DC.
	bool		m_bFlushed;

public:
	explicit CMemoryDC(CDC *pDC, LPCRECT pRect = NULL, COLORREF crBackground = CLR_DEFAULT)
		: CDC()
		, m_oldBitmap()
		, m_oldFont()
		, m_bFlushed()
	{
		ASSERT(pDC != NULL);

		m_pDC = pDC;
		m_bMemDC = !pDC->IsPrinting();

		// Get the rectangle to draw
		if (pRect == NULL)
			pDC->GetClipBox(&m_rect);
		else
			m_rect = *pRect;

		if (m_bMemDC) {
			// Create a Memory DC
			CreateCompatibleDC(pDC);

			VERIFY(m_bitmap.CreateCompatibleBitmap(pDC, m_rect.Width(), m_rect.Height()));
			m_oldBitmap = SelectObject(&m_bitmap);

			SetMapMode(pDC->GetMapMode());
			SetWindowExt(pDC->GetWindowExt());
			SetViewportExt(pDC->GetViewportExt());

			// Apply the window origin of the memory DC to the source rectangle (which is supposed to
			// refer to window origin of some other window). This way we can use the original window
			// coordinates within the memory DC without the need to explicitly adjust them.
			SetWindowOrg(m_rect.left, m_rect.top);
		} else {
			m_bPrinting = pDC->m_bPrinting;
			m_hDC = pDC->m_hDC;
			m_hAttribDC = pDC->m_hAttribDC;
		}

		FillBackground(crBackground);
	}

	virtual	~CMemoryDC()
	{
		Flush();
		if (m_oldFont)
			SelectObject(m_oldFont);
	}

	void FillBackground(COLORREF crBackground)
	{
		// Fill background - this function also sets the current background color!
		if (crBackground != CLR_NONE)
			FillSolidRect(m_rect, (crBackground == CLR_DEFAULT) ? m_pDC->GetBkColor() : crBackground);
	}

	void SetFont(CFont *pFont)
	{
		ASSERT(m_oldFont == NULL);
		m_oldFont = SelectObject(pFont);
	}

	void Flush()
	{
		if (!m_bFlushed) {
			m_bFlushed = true;
			if (m_bMemDC) {
				// Copy the off-screen bitmap onto the screen.
				m_pDC->BitBlt(m_rect.left, m_rect.top,
					m_rect.Width(), m_rect.Height(), this,
					m_rect.left, m_rect.top, SRCCOPY);

	  // Swap back the original bitmap.
				SelectObject(m_oldBitmap);
			} else
				m_hDC = m_hAttribDC = NULL;
		}
	}

	// Allow usage as a pointer
	CMemoryDC* operator->()
	{
		return this;
	}

	// Allow usage as a pointer
	operator CMemoryDC*()
	{
		return this;
	}
};