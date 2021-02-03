//***************************************************************************
//
// AUTHOR:  James White (feel free to remove or otherwise mangle any part)
//
//***************************************************************************
#include "stdafx.h"
#include "ColorButton.h"
#include "UserMsgs.h"

//***********************************************************************
//**                         MFC Debug Symbols                         **
//***********************************************************************
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//***********************************************************************
//**                            DDX Method                            **
//***********************************************************************

void AFXAPI DDX_ColorButton(CDataExchange *pDX, int nIDC, COLORREF &crColour)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	ASSERT(hWndCtrl != NULL);

	CColorButton *pColourButton = static_cast<CColorButton*>(CWnd::FromHandlePermanent(hWndCtrl));
	if (pDX->m_bSaveAndValidate)
		crColour = pColourButton->Color;
	else // initializing
		pColourButton->Color = crColour;
}

//***********************************************************************
//**                             Constants                             **
//***********************************************************************
const int g_ciArrowSizeX = 4;
const int g_ciArrowSizeY = 2;

//***********************************************************************
//**                            MFC Macros                            **
//***********************************************************************
IMPLEMENT_DYNCREATE(CColorButton, CButton)

//***********************************************************************
// Method:	CColorButton::CColorButton()
// Notes:	Default Constructor.
//***********************************************************************
CColorButton::CColorButton() :
	_Inherited(),
	m_Color(CLR_DEFAULT),
	m_DefaultColor(::GetSysColor(COLOR_APPWORKSPACE)),
	m_strDefaultText(_T("Automatic")),
	m_strCustomText(_T("More Colors...")),
	m_bPopupActive(FALSE),
	m_bTrackSelection(FALSE)
{
}

//***********************************************************************
// Method:	CColorButton::~CColorButton()
// Notes:	Destructor.
//***********************************************************************
CColorButton::~CColorButton()
{
}

//***********************************************************************
// Method:	CColorButton::GetColor()
// Notes:	None.
//***********************************************************************
COLORREF CColorButton::GetColor() const
{
	return m_Color;
}


//***********************************************************************
// Method:	CColorButton::SetColor()
// Notes:	None.
//***********************************************************************
void CColorButton::SetColor(COLORREF dwColor)
{
	m_Color = dwColor;

	if (::IsWindow(m_hWnd))
		RedrawWindow();
}


//***********************************************************************
// Method:	CColorButton::GetDefaultColor()
// Notes:	None.
//***********************************************************************
COLORREF CColorButton::GetDefaultColor() const
{
	return m_DefaultColor;
}

//***********************************************************************
// Method:	CColorButton::SetDefaultColor()
// Notes:	None.
//***********************************************************************
void CColorButton::SetDefaultColor(COLORREF dwColor)
{
	m_DefaultColor = dwColor;
}

//***********************************************************************
// Method:	CColorButton::SetCustomText()
// Notes:	None.
//***********************************************************************
void CColorButton::SetCustomText(LPCTSTR tszText)
{
	m_strCustomText = tszText;
}

//***********************************************************************
// Method:	CColorButton::SetDefaultText()
// Notes:	None.
//***********************************************************************
void CColorButton::SetDefaultText(LPCTSTR tszText)
{
	m_strDefaultText = tszText;
}


//***********************************************************************
// Method:	CColorButton::SetTrackSelection()
// Notes:	None.
//***********************************************************************
void CColorButton::SetTrackSelection(BOOL bTrack)
{
	m_bTrackSelection = bTrack;
}

//***********************************************************************
// Method:	CColorButton::GetTrackSelection()
// Notes:	None.
//***********************************************************************
BOOL CColorButton::GetTrackSelection() const
{
	return m_bTrackSelection;
}

//***********************************************************************
//**                         CButton Overrides                         **
//***********************************************************************
void CColorButton::PreSubclassWindow()
{
	ModifyStyle(0, BS_OWNERDRAW);

	_Inherited::PreSubclassWindow();
}

//***********************************************************************
//**                         Message Handlers                         **
//***********************************************************************
BEGIN_MESSAGE_MAP(CColorButton, CButton)
	//{{AFX_MSG_MAP(CColorButton)
	ON_CONTROL_REFLECT_EX(BN_CLICKED, OnClicked)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
	ON_MESSAGE(UM_CPN_SELENDOK, OnSelEndOK)
	ON_MESSAGE(UM_CPN_SELENDCANCEL, OnSelEndCancel)
	ON_MESSAGE(UM_CPN_SELCHANGE, OnSelChange)
END_MESSAGE_MAP()


//***********************************************************************
// Method:	CColorButton::OnSelEndOK()
// Notes:	None.
//***********************************************************************
LRESULT CColorButton::OnSelEndOK(WPARAM wParam, LPARAM)
{
	m_bPopupActive = FALSE;

	COLORREF OldColor = m_Color;
	Color = (COLORREF)wParam;

	CWnd *pParent = GetParent();
	if (pParent) {
		pParent->SendMessage(UM_CPN_CLOSEUP, wParam, (LPARAM)GetDlgCtrlID());
		pParent->SendMessage(UM_CPN_SELENDOK, wParam, (LPARAM)GetDlgCtrlID());
		if (OldColor != Color)
			pParent->SendMessage(UM_CPN_SELCHANGE, (m_Color != CLR_DEFAULT) ? m_Color : m_DefaultColor, (LPARAM)GetDlgCtrlID());
	}

	return TRUE;
}


//***********************************************************************
// Method:	CColorButton::OnSelEndCancel()
// Notes:	None.
//***********************************************************************
LRESULT CColorButton::OnSelEndCancel(WPARAM wParam, LPARAM)
{
	m_bPopupActive = FALSE;

	Color = (COLORREF)wParam;

	CWnd *pParent = GetParent();
	if (pParent) {
		pParent->SendMessage(UM_CPN_CLOSEUP, wParam, (LPARAM)GetDlgCtrlID());
		pParent->SendMessage(UM_CPN_SELENDCANCEL, wParam, (LPARAM)GetDlgCtrlID());
	}
	return TRUE;
}


//***********************************************************************
// Method:	CColorButton::OnSelChange()
// Notes:	None.
//***********************************************************************
LRESULT CColorButton::OnSelChange(WPARAM wParam, LPARAM)
{
	if (m_bTrackSelection)
		Color = (COLORREF)wParam;

	CWnd *pParent = GetParent();
	if (pParent)
		pParent->SendMessage(UM_CPN_SELCHANGE, (m_Color != CLR_DEFAULT) ? m_Color : m_DefaultColor, (LPARAM)GetDlgCtrlID());	//Cax2 default colour fix
	return TRUE;
}

//***********************************************************************
// Method:	CColorButton::OnCreate()
// Notes:	None.
//***********************************************************************
int CColorButton::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CButton::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

//***********************************************************************
// Method:	CColorButton::OnClicked()
// Notes:	None.
//***********************************************************************
BOOL CColorButton::OnClicked()
{
	m_bPopupActive = TRUE;

	RECT rDraw;
	GetWindowRect(&rDraw);

	new CColourPopup(CPoint(rDraw.left, rDraw.bottom)	// Point to display popup
		, m_Color							// Selected colour
		, this								// parent
		, m_strDefaultText					// "Default" text area
		, m_strCustomText);					// Custom Text

	CWnd *pParent = GetParent();

	if (pParent)
		pParent->SendMessage(UM_CPN_DROPDOWN, (WPARAM)m_Color, (LPARAM)GetDlgCtrlID());

	return TRUE;
}



//***********************************************************************
// Method:	CColorButton::DrawItem()
// Notes:	None.
//***********************************************************************
void CColorButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	ASSERT(lpDrawItemStruct);

	CDC *pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
	UINT state = lpDrawItemStruct->itemState;
	CRect rDraw = lpDrawItemStruct->rcItem;

	if (m_bPopupActive)
		state |= ODS_SELECTED | ODS_FOCUS;

	//******************************************************
	//**                  Draw Outer Edge
	//******************************************************
	UINT uFrameState = DFCS_BUTTONPUSH | DFCS_ADJUSTRECT;

	if (state & ODS_SELECTED)
		uFrameState |= DFCS_PUSHED;

	if (state & ODS_DISABLED)
		uFrameState |= DFCS_INACTIVE;

	pDC->DrawFrameControl(&rDraw, DFC_BUTTON, uFrameState);


	if (state & ODS_SELECTED)
		rDraw.OffsetRect(1, 1);

	//******************************************************
	//**                     Draw Focus
	//******************************************************
	if (state & ODS_FOCUS) {
		RECT rFocus = {rDraw.left,
					   rDraw.top,
					   rDraw.right - 1,
					   rDraw.bottom};

		pDC->DrawFocusRect(&rFocus);
	}

	int const cxedge = ::GetSystemMetrics(SM_CXEDGE);
	rDraw.DeflateRect(cxedge, ::GetSystemMetrics(SM_CYEDGE));

	//******************************************************
	//**                     Draw Arrow
	//******************************************************
	RECT rArrow;
	rArrow.left = rDraw.right - g_ciArrowSizeX - cxedge / 2;
	rArrow.right = rArrow.left + g_ciArrowSizeX;
	rArrow.top = (rDraw.bottom + rDraw.top) / 2 - g_ciArrowSizeY / 2;
	rArrow.bottom = rArrow.top + g_ciArrowSizeY;

	DrawArrow(pDC
		, &rArrow
		, 0
		, (state & ODS_DISABLED) ? ::GetSysColor(COLOR_GRAYTEXT) : RGB(0, 0, 0)
	);

	rDraw.right = rArrow.left - cxedge / 2;

	//******************************************************
	//**                   Draw Separator
	//******************************************************
	pDC->DrawEdge(&rDraw
		, EDGE_ETCHED
		, BF_RIGHT);

	rDraw.right -= (cxedge * 2) + 1;

	//******************************************************
	//**                     Draw Color
	//******************************************************
	if ((state & ODS_DISABLED) == 0) {
		pDC->FillSolidRect(&rDraw,
			(m_Color == CLR_DEFAULT)
			? m_DefaultColor
			: m_Color);

		FrameRect(pDC->m_hDC,
			&rDraw,
			(HBRUSH)GetStockObject(BLACK_BRUSH));
	}
}


//***********************************************************************
//**                          Static Methods                          **
//***********************************************************************

//***********************************************************************
// Method:	CColorButton::DrawArrow()
// Notes:	None.
//***********************************************************************
void CColorButton::DrawArrow(CDC *pDC,
	LPRECT pRect,
	int iDirection,
	COLORREF clrArrow /*= RGB(0,0,0)*/)
{
	POINT ptsArrow[3];

	switch (iDirection) {
	case 0: // Down
		ptsArrow[0].x = pRect->left;
		ptsArrow[0].y = pRect->top;
		ptsArrow[1].x = pRect->right;
		ptsArrow[1].y = pRect->top;
		ptsArrow[2].x = (pRect->left + pRect->right) / 2;
		ptsArrow[2].y = pRect->bottom;
		break;

	case 1: // Up
		ptsArrow[0].x = pRect->left;
		ptsArrow[0].y = pRect->bottom;
		ptsArrow[1].x = pRect->right;
		ptsArrow[1].y = pRect->bottom;
		ptsArrow[2].x = (pRect->left + pRect->right) / 2;
		ptsArrow[2].y = pRect->top;
		break;

	case 2: // Left
		ptsArrow[0].x = pRect->right;
		ptsArrow[0].y = pRect->top;
		ptsArrow[1].x = pRect->right;
		ptsArrow[1].y = pRect->bottom;
		ptsArrow[2].x = pRect->left;
		ptsArrow[2].y = (pRect->top + pRect->bottom) / 2;
		break;

	case 3: // Right
		ptsArrow[0].x = pRect->left;
		ptsArrow[0].y = pRect->top;
		ptsArrow[1].x = pRect->left;
		ptsArrow[1].y = pRect->bottom;
		ptsArrow[2].x = pRect->right;
		ptsArrow[2].y = (pRect->top + pRect->bottom) / 2;
		break;

	default:
		return;
	}

	CBrush brsArrow(clrArrow);
	CPen penArrow(PS_SOLID, 1, clrArrow);

	CBrush *pOldBrush = pDC->SelectObject(&brsArrow);
	CPen *pOldPen = pDC->SelectObject(&penArrow);

	pDC->SetPolyFillMode(WINDING);
	pDC->Polygon(ptsArrow, 3);

	pDC->SelectObject(pOldBrush);
	pDC->SelectObject(pOldPen);
}