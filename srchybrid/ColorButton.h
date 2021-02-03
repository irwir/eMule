//***************************************************************************
//
// AUTHOR:  James White (feel free to remove or otherwise mangle any part)
//
// DESCRIPTION: This class is alarmingly similar to the CColourPicker control
//	created by Chris Maunder of www.codeproject.com. It is so as it was blatantly
//	copied from that class and is entirely dependent on his other great work
//  in CColourPopup. I was hoping for (cough... gag...) a more Microsoft look
//  and I think this is pretty close. Hope you like it.
//
// ORIGINAL: http://www.codeproject.com/miscctrl/colour_picker.asp
//
//***************************************************************************
#pragma once
#include "ColourPopup.h"

void AFXAPI DDX_ColorButton(CDataExchange *pDX, int nIDC, COLORREF &crColour);

class CColorButton : public CButton
{
public:
	DECLARE_DYNCREATE(CColorButton);

	//***********************************************************************
	// Name:		CColorButton
	// Description:	Default constructor.
	// Parameters:	None.
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	CColorButton();

	//***********************************************************************
	// Name:		CColorButton
	// Description:	Destructor.
	// Parameters:	None.
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	virtual	~CColorButton();

	//***********************************************************************
	//**                        Property Accessors                         **
	//***********************************************************************
	__declspec(property(get=GetColor,put=SetColor))						COLORREF	Color;
	__declspec(property(get=GetDefaultColor,put=SetDefaultColor))		COLORREF	DefaultColor;
	__declspec(property(get=GetTrackSelection,put=SetTrackSelection))	BOOL		TrackSelection;
	__declspec(property(put=SetCustomText))								LPCTSTR		CustomText;
	__declspec(property(put=SetDefaultText))							LPCTSTR		DefaultText;

	//***********************************************************************
	// Name:		GetColor
	// Description:	Returns the current color selected in the control.
	// Parameters:	void
	// Return:		COLORREF
	// Notes:		None.
	//***********************************************************************
	COLORREF GetColor() const;

	//***********************************************************************
	// Name:		SetColor
	// Description:	Sets the current color selected in the control.
	// Parameters:	COLORREF Color
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	void SetColor(COLORREF dwColor);


	//***********************************************************************
	// Name:		GetDefaultColor
	// Description:	Returns the color associated with the 'default' selection.
	// Parameters:	void
	// Return:		COLORREF
	// Notes:		None.
	//***********************************************************************
	COLORREF GetDefaultColor() const;

	//***********************************************************************
	// Name:		SetDefaultColor
	// Description:	Sets the color associated with the 'default' selection.
	//				The default value is COLOR_APPWORKSPACE.
	// Parameters:	COLORREF Color
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	void SetDefaultColor(COLORREF dwColor);

	//***********************************************************************
	// Name:		SetCustomText
	// Description:	Sets the text to display in the 'Custom' selection of the
	//				CColourPicker control, the default text is "More Colors...".
	// Parameters:	LPCTSTR tszText
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	void SetCustomText(LPCTSTR tszText);

	//***********************************************************************
	// Name:		SetDefaultText
	// Description:	Sets the text to display in the 'Default' selection of the
	//				CColourPicker control, the default text is "Automatic". If
	//				this value is set to "", the 'Default' selection will not
	//				be shown.
	// Parameters:	LPCTSTR tszText
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	void SetDefaultText(LPCTSTR tszText);

	//***********************************************************************
	// Name:		SetTrackSelection
	// Description:	Turns on/off the 'Track Selection' option of the control
	//				which shows the colors during the process of selection.
	// Parameters:	BOOL bTrack
	// Return:		None.
	// Notes:		None.
	//***********************************************************************
	void SetTrackSelection(BOOL bTrack);

	//***********************************************************************
	// Name:		GetTrackSelection
	// Description:	Returns the state of the 'Track Selection' option.
	// Parameters:	void
	// Return:		BOOL
	// Notes:		None.
	//***********************************************************************
	BOOL GetTrackSelection() const;

	//{{AFX_VIRTUAL(CColorButton)
public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	protected:
	virtual void PreSubclassWindow();
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CColorButton)
	afx_msg BOOL OnClicked();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	afx_msg LRESULT OnSelEndOK(WPARAM wParam, LPARAM);
	afx_msg LRESULT OnSelEndCancel(WPARAM wParam, LPARAM);
	afx_msg LRESULT OnSelChange(WPARAM wParam, LPARAM);

	//***********************************************************************
	// Name:		DrawArrow
	// Description:	None.
	// Parameters:	CDC *pDC
	//				LPRECT pRect
	//				int iDirection
	//					0 - Down
	//					1 - Up
	//					2 - Left
	//					3 - Right
	// Return:		static None.
	// Notes:		None.
	//***********************************************************************
	static void DrawArrow(CDC *pDC,
						  LPRECT pRect,
						  int iDirection = 0,
						  COLORREF clrArrow = RGB(0,0,0));


	DECLARE_MESSAGE_MAP()

	COLORREF m_Color;
	COLORREF m_DefaultColor;
	CString m_strDefaultText;
	CString m_strCustomText;
	BOOL	m_bPopupActive;
	BOOL	m_bTrackSelection;

private:

	typedef CButton _Inherited;

};