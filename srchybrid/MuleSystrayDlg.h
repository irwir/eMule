#pragma once

class CInputBox : public CEdit
{
protected:
	afx_msg void OnContextMenu(CWnd*, CPoint);

	DECLARE_MESSAGE_MAP()
};

#include "TrayMenuBtn.h"		// Added by ClassView
#include "GradientStatic.h"	// Added by ClassView
#include "resource.h"


/////////////////////////////////////////////////////////////////////////////
// CMuleSystrayDlg dialog

class CMuleSystrayDlg : public CDialog
{
	enum
	{
		IDD = IDD_MULETRAYDLG
	};

// Construction
public:
	CMuleSystrayDlg(CWnd *pParent, CPoint pt, int iMaxUp, int iMaxDown, int iCurUp, int iCurDown);
	~CMuleSystrayDlg();

	//{{AFX_DATA(CMuleSystrayDlg)
	CStatic	m_ctrlUpArrow;
	CStatic	m_ctrlDownArrow;
	CGradientStatic	m_ctrlSidebar;
	CSliderCtrl	m_ctrlUpSpeedSld;
	CSliderCtrl	m_ctrlDownSpeedSld;
	CInputBox	m_DownSpeedInput;
	CInputBox	m_UpSpeedInput;
	uint32		m_nDownSpeedTxt;
	uint32		m_nUpSpeedTxt;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMuleSystrayDlg)
protected:
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	CTrayMenuBtn m_ctrlSpeed;
	CTrayMenuBtn m_ctrlAllToMax;
	CTrayMenuBtn m_ctrlAllToMin;
	CTrayMenuBtn m_ctrlRestore;
	CTrayMenuBtn m_ctrlDisconnect;
	CTrayMenuBtn m_ctrlConnect;
	CTrayMenuBtn m_ctrlExit;
	CTrayMenuBtn m_ctrlPreferences;

	bool m_bClosingDown;

	int m_iMaxUp;
	int m_iMaxDown;
	CPoint m_ptInitialPosition;

	HICON m_hUpArrow;
	HICON m_hDownArrow;

	UINT m_nExitCode;

	// Generated message map functions
	//{{AFX_MSG(CMuleSystrayDlg)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeDowntxt();
	afx_msg void OnChangeUptxt();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};