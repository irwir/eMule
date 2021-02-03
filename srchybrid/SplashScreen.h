#pragma once

class CSplashScreen : public CDialog
{
	DECLARE_DYNAMIC(CSplashScreen)

	enum
	{
		IDD = IDD_SPLASH
	};

public:
	explicit CSplashScreen(CWnd *pParent = NULL);   // standard constructor
	virtual	~CSplashScreen();

protected:
	CBitmap m_imgSplash;

	BOOL OnInitDialog();
	void OnPaint();
	BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()
};