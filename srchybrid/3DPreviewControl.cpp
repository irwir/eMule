#include "stdafx.h"
#include "3DPreviewControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CBarShader C3DPreviewControl::s_preview(16, 32);

// C3DPreviewControl
IMPLEMENT_DYNAMIC(C3DPreviewControl, CStatic)

BEGIN_MESSAGE_MAP(C3DPreviewControl, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()

C3DPreviewControl::C3DPreviewControl()
	: m_iSliderPos() // use flat
{
}

// Sets "slider" position for type of preview
void C3DPreviewControl::SetSliderPos(int iPos)
{
	if (iPos <= 5 && iPos >= -5)
		m_iSliderPos = iPos;
	if (GetSafeHwnd()) {
		Invalidate();
		UpdateWindow();
	}
}

void C3DPreviewControl::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	CBrush gdiBrush(RGB(104, 104, 104));
	CBrush *pOldBrush = dc.SelectObject(&gdiBrush);	//eklmn: select a new brush

	static const RECT outline_rec = { 0, 0, 34, 18 };
	dc.FrameRect(&outline_rec, &gdiBrush);
	dc.SelectObject(pOldBrush);						//eklmn: recover an old brush
	s_preview.SetFileSize((EMFileSize)32u);
	s_preview.Fill(RGB(192, 192, 255));
	s_preview.DrawPreview(&dc, 1, 1, m_iSliderPos);
}