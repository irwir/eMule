//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once
#include "MeterIcon.h"
#include "TaskbarNotifier.h"
#include "TitleMenu.h"
#include "TrayDialog.h"

namespace Kademlia
{
	class CSearch;
	class CContact;
	class CEntry;
	class CUInt128;
};

class CChatWnd;
class CIrcWnd;
class CKademliaWnd;
class CKnownFileList;
class CMainFrameDropTarget;
class CMiniMule;
class CMuleStatusBarCtrl;
class CMuleSystrayDlg;
class CMuleToolbarCtrl;
class CPreferencesDlg;
class CSearchDlg;
class CServerWnd;
class CSharedFilesWnd;
class CSplashScreen;
class CStatisticsDlg;
class CTaskbarNotifier;
class CTransferDlg;
struct Status;

// emuleapp <-> emuleapp
#define OP_ED2KLINK				12000
#define OP_CLCOMMAND			12001
#define OP_COLLECTION			12002

#define	EMULE_HOTMENU_ACCEL		'x'
#define	EMULSKIN_BASEEXT		_T("eMuleSkin")

class CemuleDlg : public CTrayDialog
{
	friend class CMuleToolbarCtrl;
	friend class CMiniMule;

	enum
	{
		IDD = IDD_EMULE_DIALOG
	};

public:
	explicit CemuleDlg(CWnd *pParent = NULL);
	~CemuleDlg();

	void ShowConnectionState();
	void ShowNotifier(LPCTSTR pszText, TbnMsg nMsgType, LPCTSTR pszLink = NULL, bool bForceSoundOFF = false);
	void SendNotificationMail(TbnMsg nMsgType, LPCTSTR pszText);
	void ShowUserCount();
	void ShowMessageState(UINT iconnr);
	void SetActiveDialog(CWnd *dlg);
	CWnd* GetActiveDialog() const			{ return activewnd; }
	void ShowTransferRate(bool bForceAll = false);
	void ShowPing();
	void Localize();

#ifdef HAVE_WIN7_SDK_H
	void UpdateStatusBarProgress();
	void UpdateThumbBarButtons(bool initialAddToDlg = false);
	void OnTBBPressed(UINT id);
	void EnableTaskbarGoodies(bool enable);

	enum TBBIDS
	{
		TBB_FIRST,
		TBB_CONNECT = TBB_FIRST,
		TBB_DISCONNECT,
		TBB_THROTTLE,
		TBB_UNTHROTTLE,
		TBB_PREFERENCES,
		TBB_LAST = TBB_PREFERENCES
	};
#endif

	// Logging
	void AddLogText(UINT uFlags, LPCTSTR pszText);
	void AddServerMessageLine(UINT uFlags, LPCTSTR pszLine);
	void ResetLog();
	void ResetDebugLog();
	void ResetServerInfo();
	CString GetLastLogEntry();
	CString	GetLastDebugLogEntry();
	CString	GetAllLogEntries();
	CString	GetAllDebugLogEntries();
	CString GetServerInfoText();

	CString	GetConnectionStateString();
	UINT GetConnectionStateIconIndex() const;
	CString	GetTransferRateString();
	CString	GetUpDatarateString(UINT uUpDatarate = UINT_MAX);
	CString	GetDownDatarateString(UINT uDownDatarate = UINT_MAX);

	void StopTimer();
	void DoVersioncheck(bool manual);
	void ApplyHyperTextFont(LPLOGFONT pFont);
	void ApplyLogFont(LPLOGFONT pFont);
	void ProcessED2KLink(LPCTSTR pszData);
	void SetStatusBarPartsSize();
	INT_PTR	ShowPreferences(UINT uStartPageID = UINT_MAX);
	bool IsPreferencesDlgOpen() const;
	bool IsTrayIconToFlash()				{ return m_iMsgIcon != 0; }
	void SetToolTipsDelay(UINT uMilliseconds);
	void StartUPnP(bool bReset = true, uint16 nForceTCPPort = 0, uint16 nForceUDPPort = 0);
	void RefreshUPnP(bool bRequestAnswer = false);
	HBRUSH GetCtlColor(CDC*, CWnd*, UINT);

	virtual void OnTrayRButtonUp(CPoint);
	virtual void OnTrayLButtonUp(CPoint pt);
	virtual void TrayMinimizeToTrayChange();
	virtual void RestoreWindow();
	virtual void HtmlHelp(DWORD_PTR dwData, UINT nCmd = 0x000F);

	CTransferDlg	*transferwnd;
	CServerWnd		*serverwnd;
	CPreferencesDlg	*preferenceswnd;
	CSharedFilesWnd	*sharedfileswnd;
	CSearchDlg		*searchwnd;
	CChatWnd		*chatwnd;
	CMuleStatusBarCtrl *statusbar;
	CStatisticsDlg	*statisticswnd;
	CIrcWnd			*ircwnd;
	CTaskbarNotifier *m_wndTaskbarNotifier;
	CReBarCtrl		m_ctlMainTopReBar;
	CMuleToolbarCtrl *toolbar;
	CKademliaWnd	*kademliawnd;
	CSplashScreen	*m_pSplashWnd;
	CWnd			*activewnd;
	uint8			status;

protected:
	HICON			m_hIcon;
	bool			ready;
	bool			m_bStartMinimizedChecked;
	bool			m_bStartMinimized;
	WINDOWPLACEMENT m_wpFirstRestore;
	HICON			m_connicons[9];
	HICON			transicons[4];
	HICON			imicons[3];
	HICON			m_icoSysTrayCurrent;
	HICON			usericon;
	CMeterIcon		m_TrayIcon;
	HICON			m_icoSysTrayConnected;		// do not use this icon for anything else than the system tray!!!
	HICON			m_icoSysTrayDisconnected;	// do not use this icon for anything else than the system tray!!!
	HICON			m_icoSysTrayLowID;			// do not use this icon for anything else than the system tray!!!
	int				m_iMsgIcon;
	UINT			m_uLastSysTrayIconCookie;
	uint32			m_uUpDatarate;
	uint32			m_uDownDatarate;
	CImageList		imagelist;
	CTitleMenu		trayPopup;
	CMuleSystrayDlg	*m_pSystrayDlg;
	CMainFrameDropTarget	*m_pDropTarget;
	CMenu			m_SysMenuOptions;
	CMenu			m_menuUploadCtrl;
	CMenu			m_menuDownloadCtrl;
	char			m_acVCDNSBuffer[MAXGETHOSTSTRUCT];
	bool			m_iMsgBlinkState;
	bool			m_bConnectRequestDelayedForUPnP;
	bool			m_bKadSuspendDisconnect;
	bool			m_bEd2kSuspendDisconnect;
	bool			m_bInitedCOM;
#ifdef HAVE_WIN7_SDK_H
	CComPtr<ITaskbarList3>	m_pTaskbarList;
	THUMBBUTTON		m_thbButtons[TBB_LAST + 1];

	TBPFLAG			m_currentTBP_state;
	float			m_prevProgress;
	HICON			m_ovlIcon;
#endif

	// Splash screen
	DWORD m_dwSplashTime;
	void ShowSplash();
	void DestroySplash();

	// Mini Mule
	CMiniMule	*m_pMiniMule;
	void DestroyMiniMule();

	CMap<UINT, UINT, LPCTSTR, LPCTSTR> m_mapTbarCmdToIcon;
	void CreateToolbarCmdIconMap();
	LPCTSTR GetIconFromCmdId(UINT uId);

	// Startup Timer
	UINT_PTR m_hTimer;
	static void CALLBACK StartupTimer(HWND hwnd, UINT uiMsg, UINT_PTR idEvent, DWORD dwTime) noexcept;

	// UPnP TimeOutTimer
	UINT_PTR m_hUPnPTimeOutTimer;
	static void CALLBACK UPnPTimeOutTimer(HWND hwnd, UINT uiMsg, UINT_PTR idEvent, DWORD dwTime) noexcept;

	void StartConnection();
	void CloseConnection();
	void MinimizeWindow();
	void PostStartupMinimized();
	void UpdateTrayIcon(int iPercent);
	void ShowConnectionStateIcon();
	void ShowTransferStateIcon();
	void ShowUserStateIcon();
	void AddSpeedSelectorMenus(CMenu *addToMenu);
	int  GetRecMaxUpload();
	void LoadNotifier(const CString &configuration);
	bool notifierenabled;
	void ShowToolPopup(bool toolsonly = false);
	void SetAllIcons();
	bool CanClose();
	int MapWindowToToolbarButton(CWnd *pWnd) const;
	CWnd* MapToolbarButtonToWindow(int iButtonID) const;
	int GetNextWindowToolbarButton(int iButtonID, int iDirection = 1) const;
	bool IsWindowToolbarButton(int iButtonID) const;
	void SetTaskbarIconColor();

	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedConnect();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnBnClickedHotmenu();
	afx_msg LRESULT OnMenuChar(UINT nChar, UINT nFlags, CMenu *pMenu);
	afx_msg void OnSysColorChange();
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg BOOL OnDeviceChange(UINT nEventType, DWORD_PTR dwData);
	afx_msg BOOL OnQueryEndSession();
	afx_msg void OnEndSession(BOOL bEnding);
	afx_msg LRESULT OnUserChanged(WPARAM, LPARAM);
	afx_msg LRESULT OnKickIdle(WPARAM, LPARAM lIdleCount);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg BOOL OnChevronPushed(UINT id, LPNMHDR pNMHDR, LRESULT *plResult);
	afx_msg LRESULT OnPowerBroadcast(WPARAM wParam, LPARAM lParam);

	// quick-speed changer -- based on xrmb
	afx_msg void QuickSpeedUpload(UINT nID);
	afx_msg void QuickSpeedDownload(UINT nID);
	afx_msg void QuickSpeedOther(UINT nID);
	// end of quick-speed changer

	afx_msg LRESULT OnTaskbarNotifierClicked(WPARAM, LPARAM lParam);
	afx_msg LRESULT OnWMData(WPARAM, LPARAM lParam);
	afx_msg LRESULT OnFileHashed(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHashFailed(WPARAM, LPARAM lParam);
	afx_msg LRESULT OnFileAllocExc(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnFileCompleted(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnFileOpProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnImportPart(WPARAM wParam,LPARAM lParam);

	//Frame grabbing
	afx_msg LRESULT OnFrameGrabFinished(WPARAM wParam, LPARAM lParam);

	afx_msg LRESULT OnAreYouEmule(WPARAM, LPARAM);

#ifdef HAVE_WIN7_SDK_H
	afx_msg LRESULT OnTaskbarBtnCreated(WPARAM, LPARAM);
#endif

	//Web Interface
	afx_msg LRESULT OnWebGUIInteraction(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWebServerClearCompleted(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWebServerFileRename(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWebAddDownloads(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnWebSetCatPrio(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAddRemoveFriend(WPARAM wParam, LPARAM lParam);

	// VersionCheck DNS
	afx_msg LRESULT OnVersionCheckResponse(WPARAM, LPARAM lParam);

	// Peercache DNS
	afx_msg LRESULT OnPeerCacheResponse(WPARAM wParam, LPARAM lParam);

	// MiniMule
	afx_msg LRESULT OnCloseMiniMule(WPARAM wParam, LPARAM);

	// Terminal Services
	afx_msg LRESULT OnConsoleThreadEvent(WPARAM wParam, LPARAM lParam);

	// UPnP
	afx_msg LRESULT OnUPnPResult(WPARAM wParam, LPARAM lParam);
};


enum EEMuleAppMsgs
{
	//thread messages
	TM_FINISHEDHASHING = WM_APP + 10,
	TM_HASHFAILED,
	TM_IMPORTPART,
	TM_FRAMEGRABFINISHED,
	TM_FILEALLOCEXC,
	TM_FILECOMPLETED,
	TM_FILEOPPROGRESS,
	TM_CONSOLETHREADEVENT
};

enum EWebinterfaceOrders
{
	WEBGUIIA_UPDATEMYINFO = 1,
	WEBGUIIA_WINFUNC,
	WEBGUIIA_UPD_CATTABS,
	WEBGUIIA_UPD_SFUPDATE,
	WEBGUIIA_UPDATESERVER,
	WEBGUIIA_STOPCONNECTING,
	WEBGUIIA_CONNECTTOSERVER,
	WEBGUIIA_DISCONNECT,
	WEBGUIIA_SERVER_REMOVE,
	WEBGUIIA_SHARED_FILES_RELOAD,
	WEBGUIIA_ADD_TO_STATIC,
	WEBGUIIA_REMOVE_FROM_STATIC,
	WEBGUIIA_UPDATESERVERMETFROMURL,
	WEBGUIIA_SHOWSTATISTICS,
	WEBGUIIA_DELETEALLSEARCHES,
	WEBGUIIA_KAD_BOOTSTRAP,
	WEBGUIIA_KAD_START,
	WEBGUIIA_KAD_STOP,
	WEBGUIIA_KAD_RCFW
};