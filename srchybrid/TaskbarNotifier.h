// CTaskbarNotifier Header file
// By John O'Byrne - 15 July 2002
// Modified by kei-kun
#pragma once

#define TN_TEXT_NORMAL			0x0000
#define TN_TEXT_BOLD			0x0001
#define TN_TEXT_ITALIC			0x0002
#define TN_TEXT_UNDERLINE		0x0004

//START - enkeyDEV(kei-kun) -TaskbarNotifier-
enum TbnMsg
{
	TBN_NONOTIFY,
	TBN_NULL,
	TBN_CHAT,
	TBN_DOWNLOADFINISHED,
	TBN_LOG,
	TBN_IMPORTANTEVENT,
	TBN_NEWVERSION,
	TBN_DOWNLOADADDED
};
//END - enkeyDEV(kei-kun) -TaskbarNotifier-


///////////////////////////////////////////////////////////////////////////////
// CTaskbarNotifierHistory

class CTaskbarNotifierHistory : public CObject
{
public:
	CTaskbarNotifierHistory()
		: m_nMessageType(TBN_NONOTIFY)
	{
	}

	virtual	~CTaskbarNotifierHistory() = default;

	CString m_strMessage;
	CString m_strLink;
	TbnMsg m_nMessageType;
};


///////////////////////////////////////////////////////////////////////////////
// CTaskbarNotifier

class CTaskbarNotifier : public CWnd
{
	DECLARE_DYNAMIC(CTaskbarNotifier)
public:
	CTaskbarNotifier();
	virtual	~CTaskbarNotifier();

	int Create(CWnd *pWndParent);
	bool LoadConfiguration(LPCTSTR pszFilePath);
	void Show(LPCTSTR pszCaption, TbnMsg nMsgType, LPCTSTR pszLink, BOOL bAutoClose = TRUE);
	void ShowLastHistoryMessage();
	int GetMessageType();
	void Hide();

	void SetBitmapRegion(int red, int green, int blue);
	bool SetBitmap(UINT nBitmapID, int red = -1, int green = -1, int blue = -1);
	bool SetBitmap(LPCTSTR pszFileName, int red = -1, int green = -1, int blue = -1);
	bool SetBitmap(CBitmap *pBitmap, int red, int green, int blue);

	void SetTextFont(LPCTSTR pszFont, int nSize, int nNormalStyle, int nSelectedStyle);
	void SetTextDefaultFont();
	void SetTextColor(COLORREF crNormalTextColor, COLORREF crSelectedTextColor);
	void SetTextRect(const RECT &rcText);
	void SetCloseBtnRect(const RECT &rcCloseBtn);
	void SetHistoryBtnRect(const RECT &rcHistoryBtn);
	void SetTextFormat(UINT uTextFormat);
	void SetAutoClose(bool bAutoClose);

protected:
	CString m_strConfigFilePath;
	time_t m_tConfigFileLastModified;
	CWnd *m_pWndParent;
	CFont m_fontNormal;
	CFont m_fontSelected;
	COLORREF m_crNormalTextColor;
	COLORREF m_crSelectedTextColor;
	HCURSOR m_hCursor;
	CBitmap m_bitmapBackground;
	HRGN m_hBitmapRegion;
	CString m_strCaption;
	CString m_strLink;
	CRect m_rcText;
	CRect m_rcCloseBtn;
	CRect m_rcHistoryBtn;
	CPoint m_ptMousePosition;
	UINT_PTR m_nAnimStatus;
	UINT m_uTextFormat;
	DWORD m_dwTimerPrecision;
	DWORD m_dwTimeToStay;
	DWORD m_dwShowEvents;
	DWORD m_dwHideEvents;
	DWORD m_dwTimeToShow;
	DWORD m_dwTimeToHide;
	bool m_bBitmapAlpha;
	bool m_bMouseIsOver;
	bool m_bTextSelected;
	bool m_bAutoClose;
	int m_nBitmapWidth;
	int m_nBitmapHeight;
	int m_nTaskbarPlacement;
	int m_nCurrentPosX;
	int m_nCurrentPosY;
	int m_nCurrentWidth;
	int m_nCurrentHeight;
	int m_nIncrementShow;
	int m_nIncrementHide;
	int m_nHistoryPosition;		  //<<--enkeyDEV(kei-kun) -TaskbarNotifier-
	TbnMsg m_nActiveMessageType;  //<<--enkeyDEV(kei-kun) -TaskbarNotifier-
	CObList m_MessageHistory;	  //<<--enkeyDEV(kei-kun) -TaskbarNotifier-
	HMODULE m_hMsImg32Dll;
	BOOL(WINAPI *m_pfnAlphaBlend)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

	HRGN CreateRgnFromBitmap(HBITMAP hBmp, COLORREF color);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseHover(WPARAM, LPARAM lParam);
	afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnPaint();
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnSysColorChange();
};