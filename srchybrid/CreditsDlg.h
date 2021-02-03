#pragma once
#include "GDIThread.h"

/////////////////////////////////////////////////////////////////////////////
// CCreditsDlg dialog

class CCreditsDlg : public CDialog
{
	enum
	{
		IDD = IDD_ABOUTBOX
	};

// Construction
public:
	void KillThread();
	void StartThread();
	explicit CCreditsDlg(CWnd *pParent = NULL);   // standard constructor
	~CCreditsDlg();

	CClientDC	*m_pDC;
	CGDIThread	*m_pThread;

// Implementation
protected:
	virtual BOOL OnInitDialog();
	virtual void OnPaint();

	BOOL PreTranslateMessage(MSG *pMsg);
	// Generated message map functions
	//{{AFX_MSG(CCreditsDlg)
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	static const RECT	m_rectScreen;
	CBitmap m_imgSplash;
};