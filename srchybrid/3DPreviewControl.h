#pragma once

#include "barshader.h"

// C3DPreviewControl

class C3DPreviewControl : public CStatic
{
	DECLARE_DYNAMIC(C3DPreviewControl)

public:
	C3DPreviewControl();
	virtual	~C3DPreviewControl() = default;

protected:
	DECLARE_MESSAGE_MAP()
	int m_iSliderPos;
	static CBarShader s_preview;
public:
	// Sets "slider" position for type of preview
	void SetSliderPos(int iPos);
	afx_msg void OnPaint();
};