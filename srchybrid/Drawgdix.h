#pragma once

/*******************************************************************

GDI Helper Classes, MFC Version, by G. Bavestrelli, Techint S.p.A.
Any feedback is welcome, you can contact me at:
					  giovanni.bavestrelli@pomini.it

class CSelStock
class CSelPen
class CSelBrush
class CSelFont
class CSelBitmap
class CSelPalette
class CSelROP2
class CSelBkMode
class CSelBkColor
class CSelTextColor
class CSelTextAlign
class CSelMapMode
class CSaveDC

class CSelStock
	CSelStock(CDC *pDC, int index)
	void Select(int index)
	CGdiObject* Old()

class CSelPen
	CSelPen(CDC *pDC, COLORREF col, int sty=PS_SOLID, int wid = 0)
	CSelPen(CDC *pDC, CPen *pPen)
	void Select(CPen *pPen)
	void Select(COLORREF col, int sty=PS_SOLID, int wid = 0)

class CSelBrush
	CSelBrush(CDC *pDC, CBrush *pBrush)
	CSelBrush(CDC *pDC, COLORREF crColor)
	CSelBrush(CDC *pDC, int index, COLORREF crColor)    // Hatch brush
	CSelBrush(CDC *pDC, CBitmap *pBitmap)              // Pattern brush
	CSelBrush(CDC *pDC, HGLOBAL hPackedDib, UINT usage) // DIB Pattern brush
	void Select(CBrush *pBrush)
	void Select(COLORREF col)
	void Select(int index, COLORREF col)                 // Hatch brush
	void Select(CBitmap *pBitmap)                       // Pattern brush
	void Select(HGLOBAL hPackedDib, UINT usage)          // DIB Pattern brush

class CSelFont
	CSelFont(CDC *pDC, int size, LPCTSTR face = NULL, BOOL bold = 0,
				BOOL italic = 0, BOOL underlined = 0, BOOL fixed = 0,
				BOOL hiquality = 0, int angleindegrees = 0)
	CSelFont(CDC *pDC, CFont *pFont)
	CSelFont(CDC *pDC, const LOGFONT *lpLogFont)

	void Select(int size, LPCTSTR face = NULL, BOOL bold = 0,
					BOOL italic = 0, BOOL underlined = 0, BOOL fixed = 0,
					BOOL hiquality = 0, int angleindegrees = 0)
	void Select(CFont *pFont)
	void Select(const LOGFONT *lpLogFont)

class CSelBitmap
	CSelBitmap(CDC *pDC, CBitmap *pBitmap)
	CSelBitmap(CDC *SelectInDC, CDC *pCompatibleToDC, int w, int h)
	void Select(CBitmap *pBitmap)
	void Select(CDC *pCompatibleToDC, int w, int h)

class CSelPalette
	CSelPalette(CDC *pDC, CPalette *pPalette,
				BOOL fForceBackground = FALSE, BOOL fRealize = TRUE)
	UINT Select(CPalette *pPalette,
				BOOL fForceBackground = FALSE, BOOL fRealize = TRUE)
	void ChangeRestoreFlags(BOOL fForceBackground, BOOL fRealize)

class CSelROP2
	CSelROP2(CDC *pDC, int drawMode)
	void Select(int drawmode)

class CSelBkMode
	CSelBkMode(CDC *pDC, int BkMode)
	void Select(int bkmode)

class CSelBkColor
	CSelBkColor(CDC *pDC, COLORREF BkColor)
	void Select(COLORREF color)

class CSelTextColor
	CSelTextColor(CDC *pDC, COLORREF TextColor)
	void Select(COLORREF color)

class CSelTextAlign
	CSelTextAlign(CDC *pDC, UINT TextAlign)
	void Select(UINT align)

class CSelMapMode
	CSelMapMode(CDC *pDC, int MapMode)
	void Select(int mode)

class CSaveDC
	CSaveDC(CDC *pDC) // saving the complete state of the DC

every class also have:
	<Constructor>(CDC *pDC) // constructor w/o selection
	void Restore()          // restores original object and destroys new object if necessary
	<retval> Old()          // returns original object

*******************************************************************/

//******************************************************************
// Base class, stores CDC *
//******************************************************************

class CSelect
{
protected:
	CDC * const m_pDC;
	explicit CSelect(CDC *pDC)
		: m_pDC(pDC)
	{
		ASSERT(m_pDC);
	}
	virtual	~CSelect() = default;

private:

	// Disable copying
	CSelect& operator=(const CSelect&) = delete;
	CSelect(const CSelect&) = delete;
};

//******************************************************************
// Class for Stock Objects
//******************************************************************

class CSelStock : public CSelect
{
	CGdiObject *m_pOldObj;

public:

	explicit CSelStock(CDC *pDC)
		: CSelect(pDC)
		, m_pOldObj()
	{
	}

	CSelStock(CDC *pDC, int index)
		: CSelect(pDC)
		, m_pOldObj()
	{
		Select(index);
	}

	~CSelStock()
	{
		Restore();
	}

	void Select(int index)
	{
		CGdiObject *pOld = m_pDC->SelectStockObject(index);
		ASSERT(pOld);
		if (!m_pOldObj)
			m_pOldObj = pOld;
	}

	void Restore()
	{
		if (m_pOldObj) {
			VERIFY(m_pDC->SelectObject(m_pOldObj));
			m_pOldObj = NULL;
		}
	}

	CGdiObject* Old() const
	{
		return m_pOldObj;
	}
};

//******************************************************************
// Pens
//******************************************************************

class CSelPen : public CSelect
{
	CPen	m_NewPen;
	CPen	*m_pOldPen;

public:

	explicit CSelPen(CDC *pDC)
		: CSelect(pDC)
		, m_pOldPen()
	{
	}

	CSelPen(CDC *pDC, COLORREF col, int sty = PS_SOLID, int wid = 0)
		: CSelect(pDC)
		, m_pOldPen()
	{
		Select(col, sty, wid);
	}

	CSelPen(CDC *pDC, CPen *pPen)
		: CSelect(pDC)
		, m_pOldPen()
	{
		Select(pPen);
	}

	~CSelPen()
	{
		Restore();
	}

	void Select(CPen *pPen)
	{
		ASSERT(pPen);
		ASSERT(pPen != &m_NewPen);
		CPen *pOld = m_pDC->SelectObject(pPen);
		ASSERT(pOld);
		if (!m_pOldPen)
			m_pOldPen = pOld;
		m_NewPen.DeleteObject();
	}

	void Select(COLORREF col, int sty = PS_SOLID, int wid = 0)
	{
		if (m_pOldPen)
			Select(m_pOldPen);
		VERIFY(m_NewPen.CreatePen(sty, wid, col));
		VERIFY(m_pOldPen = m_pDC->SelectObject(&m_NewPen));
	}

	void Restore()
	{
		if (m_pOldPen) {
			VERIFY(m_pDC->SelectObject(m_pOldPen));
			m_pOldPen = NULL;
		}
		m_NewPen.DeleteObject();
	}

	CPen* Old() const
	{
		return m_pOldPen;
	}
};

//******************************************************************
// Brushes
//******************************************************************

class CSelBrush : public CSelect
{
	CBrush	 m_NewBrush;
	CBrush *m_pOldBrush;

public:
	explicit CSelBrush(CDC *pDC)
		: CSelect(pDC)
		, m_pOldBrush()
	{
	}

	// Solid brush
	CSelBrush(CDC *pDC, COLORREF crColor)
		: CSelect(pDC)
		, m_pOldBrush()
	{
		Select(crColor);
	}

	// Hatch brush
	CSelBrush(CDC *pDC, int index, COLORREF crColor)
		: CSelect(pDC)
		, m_pOldBrush()
	{
		Select(index, crColor);
	}

	// Pattern brush
	CSelBrush(CDC *pDC, CBitmap *pBitmap)
		: CSelect(pDC)
		, m_pOldBrush()
	{
		Select(pBitmap);
	}

	// DIB Pattern brush
	CSelBrush(CDC *pDC, HGLOBAL hPackedDib, UINT usage)
		: CSelect(pDC)
		, m_pOldBrush()
	{
		Select(hPackedDib, usage);
	}

	CSelBrush(CDC *pDC, CBrush *pBrush)
		: CSelect(pDC)
		, m_pOldBrush()
	{
		Select(pBrush);
	}

	~CSelBrush()
	{
		Restore();
	}

	void Select(CBrush *pBrush)
	{
		ASSERT(pBrush);
		ASSERT(pBrush != &m_NewBrush);
		CBrush *pOld = m_pDC->SelectObject(pBrush);
		ASSERT(pOld);
		if (!m_pOldBrush)
			m_pOldBrush = pOld;
		m_NewBrush.DeleteObject();
	}

	// Solid brush
	void Select(COLORREF col)
	{
		if (m_pOldBrush)
			Select(m_pOldBrush);
		VERIFY(m_NewBrush.CreateSolidBrush(col));
		VERIFY(m_pOldBrush = m_pDC->SelectObject(&m_NewBrush));
	}

	// Hatch brush
	void Select(int index, COLORREF col)
	{
		if (m_pOldBrush)
			Select(m_pOldBrush);
		VERIFY(m_NewBrush.CreateHatchBrush(index, col));
		VERIFY(m_pOldBrush = m_pDC->SelectObject(&m_NewBrush));
	}

	// Pattern brush
	void Select(CBitmap *pBitmap)
	{
		ASSERT(pBitmap);
		if (m_pOldBrush)
			Select(m_pOldBrush);
		VERIFY(m_NewBrush.CreatePatternBrush(pBitmap));
		VERIFY(m_pOldBrush = m_pDC->SelectObject(&m_NewBrush));
	}

	// DIB Pattern brush
	void Select(HGLOBAL hPackedDib, UINT usage)
	{
		if (m_pOldBrush)
			Select(m_pOldBrush);
		VERIFY(m_NewBrush.CreateDIBPatternBrush(hPackedDib, usage));
		VERIFY(m_pOldBrush = m_pDC->SelectObject(&m_NewBrush));
	}

	void Restore()
	{
		if (m_pOldBrush) {
			VERIFY(m_pDC->SelectObject(m_pOldBrush));
			m_pOldBrush = NULL;
		}
		m_NewBrush.DeleteObject();
	}

	CBrush* Old() const
	{
		return m_pOldBrush;
	}
};



//******************************************************************
// My own font with different constructor and creation function
//******************************************************************

class CMyFont : public CFont
{
public:
	CMyFont() = default;
	CMyFont(CDC *pDC, int size, LPCTSTR face = NULL, BOOL bold = 0
		, BOOL italic = 0, BOOL underlined = 0, BOOL fixed = 0
		, BOOL hiquality = 0, int angleindegrees = 0)
	{
		VERIFY(MyCreateFont(pDC, size, face, bold, italic, underlined
			, fixed, hiquality, angleindegrees));
	}
	BOOL MyCreateFont(CDC *pDC, int size, LPCTSTR face = NULL
		, BOOL bold = 0, BOOL italic = 0, BOOL underlined = 0
		, BOOL fixed = 0, BOOL hiquality = 0
		, int angleindegrees = 0)
	{
		ASSERT(pDC);
		SIZE Size = { 0, ::MulDiv(size, pDC->GetDeviceCaps(LOGPIXELSY), 72) };
		pDC->DPtoLP(&Size);
		return CFont::CreateFont(-abs(Size.cy), 0, 10 * angleindegrees, 0
			, bold ? FW_BOLD : FW_NORMAL
			, (BYTE)italic
			, (BYTE)underlined
			, 0
			, ANSI_CHARSET
			, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS
			, (BYTE)(hiquality ? PROOF_QUALITY : DEFAULT_QUALITY)
			, (BYTE)(fixed ? FF_MODERN | FIXED_PITCH : FF_SWISS | VARIABLE_PITCH)
			, face && *face ? face : NULL);
	}
};


//******************************************************************
// Fonts
//******************************************************************


class CSelFont : public CSelect
{
	CMyFont	m_NewFont;
	CFont	*m_pOldFont;

public:

	explicit CSelFont(CDC *pDC)
		: CSelect(pDC)
		, m_pOldFont()
	{
	}

	CSelFont(CDC *pDC, int size, LPCTSTR face = NULL, BOOL bold = 0,
		BOOL italic = 0, BOOL underlined = 0, BOOL fixed = 0,
		BOOL hiquality = 0, int angleindegrees = 0)
		: CSelect(pDC)
		, m_pOldFont()
	{
		Select(size, face, bold, italic, underlined, fixed, hiquality, angleindegrees);
	}

	CSelFont(CDC *pDC, CFont *pFont)
		: CSelect(pDC)
		, m_pOldFont()
	{
		Select(pFont);
	}

	CSelFont(CDC *pDC, const LOGFONT *lpLogFont)
		: CSelect(pDC)
		, m_pOldFont()
	{
		Select(lpLogFont);
	}

	~CSelFont()
	{
		Restore();
	}

	void Select(CFont *pFont)
	{
		ASSERT(pFont);
		ASSERT(pFont != &m_NewFont);
		CFont *pOld = m_pDC->SelectObject(pFont);
		ASSERT(pOld);
		if (!m_pOldFont)
			m_pOldFont = pOld;
		m_NewFont.DeleteObject();
	}

	void Select(int size, LPCTSTR face = NULL, BOOL bold = 0,
		BOOL italic = 0, BOOL underlined = 0, BOOL fixed = 0,
		BOOL hiquality = 0, int angleindegrees = 0)
	{
		if (m_pOldFont)
			Select(m_pOldFont);
		VERIFY(m_NewFont.MyCreateFont(m_pDC, size, face, bold, italic, underlined, fixed, hiquality, angleindegrees));
		VERIFY(m_pOldFont = m_pDC->SelectObject(&m_NewFont));
	}

	void Select(const LOGFONT *lpLogFont)
	{
		if (m_pOldFont)
			Select(m_pOldFont);
		VERIFY(m_NewFont.CreateFontIndirect(lpLogFont));
		VERIFY(m_pOldFont = m_pDC->SelectObject(&m_NewFont));
	}

	void Restore()
	{
		if (m_pOldFont) {
			VERIFY(m_pDC->SelectObject(m_pOldFont));
			m_pOldFont = NULL;
		}
		m_NewFont.DeleteObject();
	}

	CFont* Old() const
	{
		return m_pOldFont;
	}
};


//******************************************************************
// Bitmaps
//******************************************************************

class CSelBitmap : public CSelect
{
	CBitmap	m_NewBmp;
	CBitmap	*m_pOldBmp;

public:
	explicit CSelBitmap(CDC *pDC)
		: CSelect(pDC)
		, m_pOldBmp()
	{
	}

	CSelBitmap(CDC *SelectInDC, CDC *pCompatibleToDC, int w, int h)
		: CSelect(SelectInDC)
		, m_pOldBmp()
	{
		Select(pCompatibleToDC, w, h);
	}

	CSelBitmap(CDC *pDC, CBitmap *pBitmap)
		: CSelect(pDC)
		, m_pOldBmp()
	{
		Select(pBitmap);
	}

	~CSelBitmap()
	{
		Restore();
	}

	void Select(CBitmap *pBitmap)
	{
		ASSERT(pBitmap);
		ASSERT(pBitmap != &m_NewBmp);
		CBitmap *pOld = m_pDC->SelectObject(pBitmap);
		ASSERT(pOld);
		if (!m_pOldBmp)
			m_pOldBmp = pOld;
		m_NewBmp.DeleteObject();
	}

	void Select(CDC *pCompatibleToDC, int w, int h)
	{
		ASSERT(pCompatibleToDC);
		if (m_pOldBmp)
			Select(m_pOldBmp);
		VERIFY(m_NewBmp.CreateCompatibleBitmap(pCompatibleToDC, w, h));
		VERIFY(m_pOldBmp = m_pDC->SelectObject(&m_NewBmp));
	}

	void Restore()
	{
		if (m_pOldBmp) {
			VERIFY(m_pDC->SelectObject(m_pOldBmp));
			m_pOldBmp = NULL;
		}
		m_NewBmp.DeleteObject();
	}

	CBitmap* Old() const
	{
		return m_pOldBmp;
	}
};

// This class is a bit different
class CSelPalette : public CSelect
{
	// You need your own palette, use CPalette
	CPalette *m_pOldPalette;
	BOOL	m_fForceBackground;
	BOOL	m_fRealizePalette;

public:

	explicit CSelPalette(CDC *pDC)
		: CSelect(pDC)
		, m_pOldPalette()
		, m_fForceBackground()
		, m_fRealizePalette()
	{
	}

	CSelPalette(CDC *pDC, CPalette *pPalette,
		BOOL fForceBackground = FALSE, BOOL fRealize = TRUE)
		: CSelect(pDC)
		, m_pOldPalette()
	{
		Select(pPalette, fForceBackground, fRealize);
	}

	~CSelPalette()
	{
		Restore();
	}

	UINT Select(CPalette *pPalette, BOOL fForceBackground = FALSE, BOOL fRealize = TRUE)
	{
		ASSERT(pPalette);
		ASSERT(m_pDC->GetDeviceCaps(RASTERCAPS)&RC_PALETTE);
		CPalette *pOld = m_pDC->SelectPalette(pPalette, fForceBackground);
		ASSERT(pOld);
		if (!m_pOldPalette)
			m_pOldPalette = pOld;
		m_fForceBackground = fForceBackground;
		m_fRealizePalette = fRealize;
		return fRealize ? m_pDC->RealizePalette() : 0;
	}

	void ChangeRestoreFlags(BOOL fForceBackground, BOOL fRealize)
	{
		m_fForceBackground = fForceBackground;
		m_fRealizePalette = fRealize;
	}

	void Restore()
	{
		if (m_pOldPalette) {
			VERIFY(m_pDC->SelectPalette(m_pOldPalette, m_fForceBackground));
			if (m_fRealizePalette)
				m_pDC->RealizePalette();
			m_pOldPalette = NULL;
		}
	}

	CPalette* Old() const
	{
		return m_pOldPalette;
	}
};


//******************************************************************
// Set and restore other characteristics of the DC (no GDI objects)
//******************************************************************


class CSelROP2 : public CSelect
{
	int m_OldRop;

public:

	explicit CSelROP2(CDC *pDC)
		: CSelect(pDC)
		, m_OldRop()
	{
		/*VERIFY(m_OldRop=m_pDC->GetROP2());*/
	}

	CSelROP2(CDC *pDC, int drawMode)
		: CSelect(pDC)
		, m_OldRop(0)
	{
		Select(drawMode);
	}

	~CSelROP2()
	{
		Restore();
	}

	void Select(int drawmode)
	{
		int old = m_pDC->SetROP2(drawmode);
		ASSERT(old);
		if (!m_OldRop)
			m_OldRop = old;
	}

	void Restore()
	{
		if (m_OldRop) {
			VERIFY(m_pDC->SetROP2(m_OldRop));
			m_OldRop = 0;
		}
	}

	int Old() const
	{
		return m_OldRop;
	}
};


class CSelBkMode : public CSelect
{
	int m_OldBkMode;

public:

	explicit CSelBkMode(CDC *pDC)
		: CSelect(pDC)
		, m_OldBkMode(0)
	{
		/*VERIFY(m_OldBkMode = m_pDC->GetBkMode());*/
	}

	CSelBkMode(CDC *pDC, int BkMode)
		: CSelect(pDC)
		, m_OldBkMode(0)
	{
		Select(BkMode);
	}

	~CSelBkMode()
	{
		Restore();
	}

	void Select(int BkMode)
	{
		int old = m_pDC->SetBkMode(BkMode);
		ASSERT(old);
		if (!m_OldBkMode)
			m_OldBkMode = old;
	}

	void Restore()
	{
		if (m_OldBkMode) {
			VERIFY(m_pDC->SetBkMode(m_OldBkMode));
			m_OldBkMode = 0;
		}
	}

	int Old() const
	{
		return m_OldBkMode;
	}
};


class CSelBkColor : public CSelect
{
	COLORREF m_OldBkColor;

public:

	explicit CSelBkColor(CDC *pDC)
		: CSelect(pDC)
		, m_OldBkColor(CLR_INVALID)
	{
		m_OldBkColor = m_pDC->GetBkColor();
	}

	CSelBkColor(CDC *pDC, COLORREF BkColor)
		: CSelect(pDC)
		, m_OldBkColor(CLR_INVALID)
	{
		Select(BkColor);
	}

	~CSelBkColor()
	{
		Restore();
	}

	void Select(COLORREF color)
	{
		ASSERT(color != CLR_INVALID);
		int old = m_pDC->SetBkColor(color);	ASSERT(old != CLR_INVALID);
		if (m_OldBkColor == CLR_INVALID)
			m_OldBkColor = old;
	}

	void Restore()
	{
		if (m_OldBkColor != CLR_INVALID) {
			VERIFY(m_pDC->SetBkColor(m_OldBkColor) != CLR_INVALID);
			m_OldBkColor = CLR_INVALID;
		}
	}

	COLORREF Old() const
	{
		return m_OldBkColor;
	}
};


class CSelTextColor : public CSelect
{
	COLORREF m_OldTextColor;

public:

	explicit CSelTextColor(CDC *pDC)
		: CSelect(pDC)
		, m_OldTextColor(m_pDC->GetTextColor())
	{
	}

	CSelTextColor(CDC *pDC, COLORREF TextColor)
		: CSelect(pDC)
		, m_OldTextColor(CLR_INVALID)
	{
		Select(TextColor);
	}

	~CSelTextColor()
	{
		Restore();
	}

	void Select(COLORREF color)
	{
		ASSERT(color != CLR_INVALID);
		int old = m_pDC->SetTextColor(color);
		ASSERT(old != CLR_INVALID);
		if (m_OldTextColor == CLR_INVALID)
			m_OldTextColor = old;
	}

	void Restore()
	{
		if (m_OldTextColor != CLR_INVALID) {
			VERIFY(m_pDC->SetTextColor(m_OldTextColor) != CLR_INVALID);
			m_OldTextColor = CLR_INVALID;
		}
	}

	COLORREF Old() const
	{
		return m_OldTextColor;
	}
};


class CSelTextAlign : public CSelect
{
	UINT m_OldTextAlign;

public:

	explicit CSelTextAlign(CDC *pDC)
		: CSelect(pDC)
		, m_OldTextAlign(m_pDC->GetTextAlign())
	{
	}

	CSelTextAlign(CDC *pDC, UINT TextAlign)
		: CSelect(pDC)
		, m_OldTextAlign(GDI_ERROR)
	{
		Select(TextAlign);
	}

	~CSelTextAlign()
	{
		Restore();
	}

	void Select(UINT align)
	{
		ASSERT(align != GDI_ERROR);
		int old = m_pDC->SetTextAlign(align);
		ASSERT(old != GDI_ERROR);
		if (m_OldTextAlign == GDI_ERROR)
			m_OldTextAlign = old;
	}

	void Restore()
	{
		if (m_OldTextAlign != GDI_ERROR) {
			VERIFY(m_pDC->SetTextAlign(m_OldTextAlign) != GDI_ERROR);
			m_OldTextAlign = GDI_ERROR;
		}
	}

	UINT Old() const
	{
		return m_OldTextAlign;
	}
};


class CSelMapMode : public CSelect
{
	int m_OldMapMode;

public:

	explicit CSelMapMode(CDC *pDC)
		: CSelect(pDC)
		, m_OldMapMode(0)
	{
		/*VERIFY(m_OldMapMode = m_pDC->GetMapMode());*/
	}

	CSelMapMode(CDC *pDC, int MapMode)
		: CSelect(pDC)
		, m_OldMapMode(0)
	{
		Select(MapMode);
	}

	~CSelMapMode()
	{
		Restore();
	}

	void Select(int MapMode)
	{
		int old = m_pDC->SetMapMode(MapMode);
		ASSERT(old);
		if (!m_OldMapMode)
			m_OldMapMode = old;
	}

	void Restore()
	{
		if (m_OldMapMode) {
			VERIFY(m_pDC->SetMapMode(m_OldMapMode));
			m_OldMapMode = 0;
		}
	}

	UINT Old() const
	{
		return m_OldMapMode;
	}
};

//******************************************************************
// Class for saving the complete state of the DC
//******************************************************************

class CSaveDC : public CSelect
{
	int m_SavedDC;

public:

	explicit CSaveDC(CDC *pDC)
		: CSelect(pDC)
	{
		VERIFY(m_SavedDC = m_pDC->SaveDC());
	}

	~CSaveDC()
	{
		Restore();
	}

	void Restore()
	{
		if (m_SavedDC) {
			VERIFY(m_pDC->RestoreDC(m_SavedDC));
			m_SavedDC = 0;
		}
	}

	int Old() const
	{
		return m_SavedDC;
	}
};