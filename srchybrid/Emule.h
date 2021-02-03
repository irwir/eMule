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
#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif
#include "resource.h"

#define	DEFAULT_NICK		thePrefs.GetHomepageBaseURL()
#define	DEFAULT_TCP_PORT_OLD	4662
#define	DEFAULT_UDP_PORT_OLD	(DEFAULT_TCP_PORT_OLD+10)

#define PORTTESTURL			_T("http://porttest.emule-project.net/connectiontest.php?tcpport=%i&udpport=%i&lang=%i")

class CSearchList;
class CUploadQueue;
class CListenSocket;
class CDownloadQueue;
class CScheduler;
class UploadBandwidthThrottler;
class LastCommonRouteFinder;
class CemuleDlg;
class CClientList;
class CKnownFileList;
class CServerConnect;
class CServerList;
class CSharedFileList;
class CClientCreditsList;
class CFriendList;
class CClientUDPSocket;
class CIPFilter;
class CWebServer;
class CAbstractFile;
class CUpDownClient;
class CPeerCacheFinder;
class CFirewallOpener;
class CUPnPImplWrapper;
class CUploadDiskIOThread;

struct SLogItem;

enum AppState
{
	APP_STATE_STARTING = 0,	//initialization phase
	APP_STATE_RUNNING,
	APP_STATE_ASKCLOSE,		//exit confirmation dialog is active
	APP_STATE_SHUTTINGDOWN,
	APP_STATE_DONE			//shutdown completed
};

class CemuleApp : public CWinApp
{
public:
	explicit CemuleApp(LPCTSTR lpszAppName = NULL);
	bool IsRunning() const;
	bool IsClosing() const;

	// ZZ:UploadSpeedSense -->
	UploadBandwidthThrottler *uploadBandwidthThrottler;
	LastCommonRouteFinder *lastCommonRouteFinder;
	// ZZ:UploadSpeedSense <--
	CemuleDlg			*emuledlg;
	CClientList			*clientlist;
	CKnownFileList		*knownfiles;
	CServerConnect		*serverconnect;
	CServerList			*serverlist;
	CSharedFileList		*sharedfiles;
	CSearchList			*searchlist;
	CListenSocket		*listensocket;
	CUploadQueue		*uploadqueue;
	CDownloadQueue		*downloadqueue;
	CClientCreditsList	*clientcredits;
	CFriendList			*friendlist;
	CClientUDPSocket	*clientudp;
	CIPFilter			*ipfilter;
	CWebServer			*webserver;
	CScheduler			*scheduler;
	CPeerCacheFinder	*m_pPeerCache;
	CFirewallOpener		*m_pFirewallOpener;
	CUPnPImplWrapper	*m_pUPnPFinder;
	CUploadDiskIOThread	*m_pUploadDiskIOThread;

	static const UINT	m_nVersionMjr;
	static const UINT	m_nVersionMin;
	static const UINT	m_nVersionUpd;
	static const UINT	m_nVersionBld;
	static const TCHAR	*m_sPlatform;

	HANDLE		m_hMutexOneInstance;
	int			m_iDfltImageListColorFlags;
	CFont		m_fontHyperText;
	CFont		m_fontDefaultBold;
	CFont		m_fontSymbol;
	CFont		m_fontLog;
	CFont		m_fontChatEdit;
	CBrush		m_brushBackwardDiagonal;
	DWORD		m_dwProductVersionMS;
	DWORD		m_dwProductVersionLS;
	CString		m_strCurVersionLong;
	CString		m_strCurVersionLongDbg;
	UINT		m_uCurVersionShort;
	UINT		m_uCurVersionCheck;
	ULONGLONG	m_ullComCtrlVer;
	CMutex		hashing_mut;
	CString		m_strPendingLink;
	COPYDATASTRUCT sendstruct;
	AppState	m_app_state; // defines application state

// Implementierung
	virtual BOOL InitInstance();
	virtual int	ExitInstance();
	virtual BOOL IsIdleMessage(MSG *pMsg);

	// ed2k link functions
	void		AddEd2kLinksToDownload(const CString &strLinks, int cat);
	void		SearchClipboard();
	void		IgnoreClipboardLinks(const CString &strLinks)	{ m_strLastClipboardContents = strLinks; }
	void		PasteClipboard(int cat = 0);
	bool		IsEd2kFileLinkInClipboard();
	bool		IsEd2kServerLinkInClipboard();
	bool		IsEd2kLinkInClipboard(LPCSTR pszLinkType, int iLinkTypeLen);
	LPCTSTR		GetProfileFile()								{ return m_pszProfileName; }

	CString		CreateKadSourceLink(const CAbstractFile *f);

	// clipboard (text)
	bool		CopyTextToClipboard(const CString &strText);
	CString		CopyTextFromClipboard();

	void		OnlineSig();
	void		UpdateReceivedBytes(uint32 bytesToAdd);
	void		UpdateSentBytes(uint32 bytesToAdd, bool sentToFriend = false);
	int			GetFileTypeSystemImageIdx(LPCTSTR pszFilePath, int iLength = -1, bool bNormalsSize = false);
	HIMAGELIST	GetSystemImageList()							{ return m_hSystemImageList; }
	HIMAGELIST	GetBigSystemImageList()							{ return m_hBigSystemImageList; }
	CSize		GetSmallSytemIconSize()							{ return m_sizSmallSystemIcon; }
	CSize		GetBigSytemIconSize()							{ return m_sizBigSystemIcon; }
	void		CreateBackwardDiagonalBrush();
	void		CreateAllFonts();
	const CString& GetDefaultFontFaceName();
	bool		IsPortchangeAllowed();
	bool		IsConnected(bool bIgnoreEd2k = false, bool bIgnoreKad = false);
	bool		IsFirewalled();
	bool		CanDoCallback(CUpDownClient *client);
	uint32		GetID();
	uint32		GetPublicIP(bool bIgnoreKadIP = false) const;	// return current (valid) public IP or 0 if unknown
	void		SetPublicIP(const uint32 dwIP);
	void		ResetStandByIdleTimer();

	// because nearly all icons we are loading are 16x16, the default size is specified as 16 and not as 32 nor LR_DEFAULTSIZE
	HICON		LoadIcon(LPCTSTR lpszResourceName, int cx = 16, int cy = 16, UINT uFlags = LR_DEFAULTCOLOR) const;
	HICON		LoadIcon(UINT nIDResource) const;
	HBITMAP		LoadImage(LPCTSTR lpszResourceName, LPCTSTR pszResourceType) const;
//	HBITMAP		LoadImage(UINT nIDResource, LPCTSTR pszResourceType) const;
	bool		LoadSkinColor(LPCTSTR pszKey, COLORREF &crColor) const;
	bool		LoadSkinColorAlt(LPCTSTR pszKey, LPCTSTR pszAlternateKey, COLORREF &crColor) const;
	CString		GetSkinFileItem(LPCTSTR lpszResourceName, LPCTSTR pszResourceType) const;
	void		ApplySkin(LPCTSTR pszSkinProfile);
	void		EnableRTLWindowsLayout();
	void		DisableRTLWindowsLayout();
	void		UpdateDesktopColorDepth();
	void		UpdateLargeIconSize();
	bool		IsXPThemeActive() const;
	bool		IsVistaThemeActive() const;

	bool		GetLangHelpFilePath(CString &strResult);
	void		SetHelpFilePath(LPCTSTR pszHelpFilePath);
	void		ShowHelp(UINT uTopic, UINT uCmd = HELP_CONTEXT);
	bool		ShowWebHelp(UINT uTopic);

	// Elandal:ThreadSafeLogging -->
	// thread safe log calls
	void		QueueDebugLogLine(bool bAddToStatusBar, LPCTSTR line, ...);
	void		QueueDebugLogLineEx(UINT uFlags, LPCTSTR line, ...);
	void		HandleDebugLogQueue();
	void		ClearDebugLogQueue(bool bDebugPendingMsgs = false);

	void		QueueLogLine(bool bAddToStatusBar, LPCTSTR line, ...);
	void		QueueLogLineEx(UINT uFlags, LPCTSTR line, ...);
	void		HandleLogQueue();
	void		ClearLogQueue(bool bDebugPendingMsgs = false);
	// Elandal:ThreadSafeLogging <--

	bool		DidWeAutoStart()								{ return m_bAutoStart; }

protected:
	bool ProcessCommandline();
	void SetTimeOnTransfer();
	static BOOL CALLBACK SearchEmuleWindow(HWND hWnd, LPARAM lParam) noexcept;

	HIMAGELIST	m_hSystemImageList;
	CMapStringToPtr m_aExtToSysImgIdx;
	CSize		m_sizSmallSystemIcon;

	HIMAGELIST	m_hBigSystemImageList;
	CMapStringToPtr m_aBigExtToSysImgIdx;
	CSize		m_sizBigSystemIcon;

	CString		m_strDefaultFontFaceName;
	CString		m_strLastClipboardContents;

	// Elandal:ThreadSafeLogging -->
	// thread safe log calls
	CCriticalSection m_queueLock;
	CTypedPtrList<CPtrList, SLogItem*> m_QueueDebugLog;
	CTypedPtrList<CPtrList, SLogItem*> m_QueueLog;
	// Elandal:ThreadSafeLogging <--

	WSADATA		m_wsaData;
	uint32		m_dwPublicIP;
	bool		m_bGuardClipboardPrompt;
	bool		m_bAutoStart;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHelp();

private:
	UINT		m_wTimerRes;
};

extern CemuleApp theApp;


//////////////////////////////////////////////////////////////////////////////
// CTempIconLoader

class CTempIconLoader
{
	HICON m_hIcon;

public:
	// because nearly all icons we are loading are 16x16, the default size is specified as 16 and not as 32 nor LR_DEFAULTSIZE
	explicit CTempIconLoader(LPCTSTR pszResourceID, int cx = 16, int cy = 16, UINT uFlags = LR_DEFAULTCOLOR);
	explicit CTempIconLoader(UINT uResourceID, int cx = 16, int cy = 16, UINT uFlags = LR_DEFAULTCOLOR);
	~CTempIconLoader();

	operator HICON() const										{ return m_hIcon; }
};