#pragma once

class CButtonsTabCtrl : public CTabCtrl
{
	DECLARE_DYNAMIC(CButtonsTabCtrl)

public:
	CButtonsTabCtrl() = default;
	virtual	~CButtonsTabCtrl() = default;

protected:
	void InternalInit();

	virtual void PreSubclassWindow();
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT _OnThemeChanged();
};