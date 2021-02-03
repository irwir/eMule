#pragma once

#include "3dpreviewcontrol.h"

class CPPgDisplay : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgDisplay)

	enum
	{
		IDD = IDD_PPG_DISPLAY
	};

public:
	CPPgDisplay();
	virtual	~CPPgDisplay() = default;

	void Localize();

protected:
	enum ESelectFont
	{
		sfServer,
		sfLog
	} m_eSelectFont;
	void LoadSettings();

	void DrawPreview();		//Cax2 - aqua bar
	C3DPreviewControl	m_3DPreview;

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	static UINT_PTR CALLBACK ChooseFontHook(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnSettingsChange()				{ SetModified(); }
	afx_msg void OnBnClickedSelectHypertextFont();
	afx_msg void OnBtnClickedResetHist();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};