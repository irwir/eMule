#include "stdafx.h"
#include "TrayMenuBtn.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CTrayMenuBtn

BEGIN_MESSAGE_MAP(CTrayMenuBtn, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_PAINT()
END_MESSAGE_MAP()

CTrayMenuBtn::CTrayMenuBtn()
	: m_sIcon()
	, m_hIcon()
	, m_nBtnID(rand())
	, m_bBold()
	, m_bMouseOver()
	, m_bNoHover()
	, m_bUseIcon()
	, m_bParentCapture()
{
}

CTrayMenuBtn::~CTrayMenuBtn()
{
	if (m_hIcon)
		::DestroyIcon(m_hIcon);
}

void CTrayMenuBtn::OnMouseMove(UINT nFlags, CPoint point)
{
	CRect rClient;
	GetClientRect(rClient);

	if (rClient.PtInRect(point)) {
		SetCapture();

		if (!m_bNoHover)
			m_bMouseOver = true;
	} else {
		if (m_bParentCapture) {
			CWnd *pParent = GetParent();
			if (pParent)
				pParent->SetCapture();
			else
				ReleaseCapture();
		} else
			ReleaseCapture();

		m_bMouseOver = false;
	}
	Invalidate();

	CWnd::OnMouseMove(nFlags, point);
}

void CTrayMenuBtn::OnLButtonUp(UINT nFlags, CPoint point)
{
	CRect rClient;
	GetClientRect(rClient);

	if (rClient.PtInRect(point)) {
		CWnd *pParent = GetParent();
		if (pParent)
			pParent->PostMessage(WM_COMMAND, MAKEWPARAM(m_nBtnID, BN_CLICKED), reinterpret_cast<LPARAM>(m_hWnd));
	} else {
		ReleaseCapture();
		m_bMouseOver = false;
		Invalidate();
	}

	CWnd::OnLButtonUp(nFlags, point);
}

void CTrayMenuBtn::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rClient;
	GetClientRect(rClient);

	CDC MemDC;
	MemDC.CreateCompatibleDC(&dc);
	CBitmap MemBMP, *pOldBMP;
	MemBMP.CreateCompatibleBitmap(&dc, rClient.Width(), rClient.Height());
	pOldBMP = MemDC.SelectObject(&MemBMP);
	CFont *pOldFONT = m_cfFont.GetSafeHandle() ? MemDC.SelectObject(&m_cfFont) : NULL;

	BOOL bEnabled = IsWindowEnabled();

	bool bClr = (m_bMouseOver && bEnabled);
	::FillRect(MemDC.m_hDC, rClient, ::GetSysColorBrush(bClr ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
	MemDC.SetTextColor(::GetSysColor(bClr ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));

	int iLeftOffset = 0;
	if (m_bUseIcon) {
		MemDC.DrawState(CPoint(2, rClient.Height() / 2 - m_sIcon.cy / 2), CSize(16, 16), m_hIcon, DST_ICON | DSS_NORMAL, (CBrush*)NULL);
		iLeftOffset = m_sIcon.cx + 4;
	}

	MemDC.SetBkMode(TRANSPARENT);
	CRect rText;
	MemDC.DrawText(m_strText, rText, DT_CALCRECT | DT_SINGLELINE | DT_LEFT);
	CPoint pt(rClient.left + 2 + iLeftOffset, rClient.Height() / 2 - rText.Height() / 2);
	CPoint sz(rText.Width(), rText.Height());
	MemDC.DrawState(pt, sz, m_strText, DST_TEXT | (bEnabled ? DSS_NORMAL : DSS_DISABLED),
		FALSE, m_strText.GetLength(), (CBrush*)NULL);
	dc.BitBlt(0, 0, rClient.Width(), rClient.Height(), &MemDC, 0, 0, SRCCOPY);
	MemDC.SelectObject(pOldBMP);
	if (pOldFONT)
		MemDC.SelectObject(pOldFONT);
}